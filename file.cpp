/*
	Untrunc - file.cpp

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.

	Untrunc is distributed under the GPL "AS IS", without
	any warranty; without the implied warranty of merchantability
	or fitness for either an expressed or implied particular purpose.

	Please see the included GNU General Public License (GPL) for
	your rights and further details; see the file COPYING. If you
	cannot, write to the Free Software Foundation, 59 Temple Place
	Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

	Copyright 2010 Federico Ponchio

							*/

#include "file.h"
#include <string>
#include <cstring>
#include <iostream>

#include "common.h"

using namespace std;

FileRead::FileRead(): file_(NULL), buf_begin_(0), buf_off_(0) {
}

FileRead::~FileRead() {
	if(file_){
		fclose(file_);
		free(buffer_);
	}
}

FileWrite::FileWrite(): file_(NULL) {
}

FileWrite::~FileWrite() {
	if(file_){
		fclose(file_);
	}
}

bool FileRead::open(string filename) {
	file_ = fopen(filename.c_str(), "rb");
	if(file_ == NULL) return false;

	fseeko(file_, 0L, SEEK_END);
	size_ = ftello64(file_);
	fseeko(file_, 0L, SEEK_SET);

	buffer_ = (uchar*) malloc(buf_size_);
	fread(buffer_, 1, buf_size_, file_);

	return true;
}

bool FileWrite::create(string filename) {
	file_ = fopen(filename.c_str(), "wb");
	if(file_ == NULL) return false;
	return true;
}

void FileRead::seek(off64_t p) {
	if (p < buf_begin_ || p >= buf_begin_ + buf_size_){
		fillBuffer(p);
	} else
		buf_off_ = p - buf_begin_;
}

void FileRead::seekSafe(off64_t p) {
	seek(min(p, size_));
}

off64_t FileRead::pos() {
	return buf_begin_ + buf_off_;
}

bool FileRead::atEnd() {
	return pos() == size_;
}

size_t FileRead::fillBuffer(off64_t location) {
	off64_t avail = (buf_begin_+buf_size_) - location;
	off64_t buf_loc = location - buf_begin_;

	buf_begin_ = location;
	buf_off_ = 0;
	if (avail < 0 || avail >= buf_size_) {
		fseeko(file_, location, SEEK_SET);
		int n = fread(buffer_, 1, buf_size_, file_);
		return n;
	}else if (avail > 0) {
		memmove(buffer_, buffer_+buf_loc, buf_size_-buf_loc);
	}
	int n = fread(buffer_+avail, 1, buf_size_-avail, file_);
	return n;
}

size_t FileRead::readBuffer(uchar* dest, size_t size, size_t n) {
	logg(VV, "requests: ", size*n, " at offset : ", buf_off_, '\n');
	size_t total = size*n;
	size_t avail = buf_size_ - buf_off_;
	size_t nread = 0;
	if (avail < total) {
		logg(VV, "reallocating the file buffer\n");
		memcpy(dest, buffer_+buf_off_, avail);
		nread = avail;
		total -= avail;
		buf_off_ = buf_size_;
		if (total >= to_uint(buf_size_)){
			size_t x = fread(dest+nread, 1, total, file_);
			nread += x;
			fillBuffer(ftello64(file_));
		} else {
			size_t x = min(fillBuffer(buf_begin_+buf_off_), total);
			memcpy(dest+nread, buffer_, x);
			buf_off_ += x;
			nread += x;
		}
	} else {
		memcpy(dest, buffer_+buf_off_, total);
		buf_off_ += total;
		nread = total;
	}
	return nread/size;
}

uint FileRead::readInt() {
	int value;
	int n = readBuffer((uchar*)&value, sizeof(int), 1);
	if(n != 1) throw "Could not read integer";
	return swap32(value);
}

int64_t FileRead::readInt64() {
	int64_t value;
	int n = readBuffer((uchar*)&value, sizeof(value), 1);
	if(n != 1)
		throw "Could not read int64";

	return swap64(value);
}

string FileRead::getString(size_t n) {
	string r;
	r.resize(n);
	readChar(&r[0], n);
	return r;
}

void FileRead::readChar(char *dest, size_t n) {
	size_t len = readBuffer((uchar*)dest, sizeof(char), n);
	if(len != n){
		cout << "expected " << n << " but got " << len << '\n';
		throw "Could not read chars";
	}
}

vector<uchar> FileRead::read(size_t n) {
	vector<uchar> dest(n);
	size_t len = readBuffer(&*dest.begin(), 1, n);
	if(len != n)
		throw "Could not read at position";
	return dest;
}

const uchar* FileRead::getPtr(int size_requested) {
	// check if requested size exceeds buffer
	if (buf_off_ + size_requested > buf_size_){
		logg(VV, "size_requested: ", size_requested, '\n');
//		cout << "buffer_:\n";
//		printBuffer(buffer_, 30);
		fillBuffer(buf_begin_+buf_off_);
//		cout << "buffer_:\n";
//		printBuffer(buffer_, 30);
//		return buffer_tmp_;
	}
	return buffer_+buf_off_;
}

const uchar* FileRead::getPtr2(int size_requested) {
	auto ret = getPtr(size_requested);
	buf_off_ += size_requested;
	return ret;
}

const uchar* FileRead::getPtrAt(off64_t pos, int size_requested) {
	seek(pos);
	auto ret = getPtr(size_requested);
	buf_off_ += size_requested;
	return ret;
}

off64_t FileWrite::pos() {
	return ftello64(file_);
}

int FileWrite::writeInt(int n) {
	n = swap32(n);
	fwrite(&n, sizeof(int), 1, file_);
	return 4;
}

int FileWrite::writeInt64(int64_t n) {
	n = swap64(n);
	fwrite(&n, sizeof(n), 1, file_);
	return 8;
}

int FileWrite::writeChar(const char *source, size_t n) {
	fwrite(source, 1, n, file_);
	return n;
}

int FileWrite::writeChar(const uchar *source, size_t n) {
	fwrite(source, 1, n, file_);
	return n;
}

int FileWrite::write(vector<uchar> &v) {
	fwrite(&*v.begin(), 1, v.size(), file_);
	return v.size();
}
