// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/fifo_char.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

namespace nacl_io {

FIFOChar::FIFOChar(size_t size)
    : buffer_(NULL), size_(size), avail_(0), tail_(0) {
  if (size) {
    buffer_ = (char*)malloc(size);
    assert(buffer_ != NULL);
  }
}

FIFOChar::~FIFOChar() {
  free(buffer_);
}

bool FIFOChar::IsEmpty() {
  return avail_ == 0;
}

bool FIFOChar::IsFull() {
  return avail_ >= size_;
}

bool FIFOChar::Resize(size_t len) {
  // Can not resize smaller than the current size
  if (len < avail_)
    return false;

  // Resize buffer
  buffer_ = (char*)realloc(buffer_, len);
  assert(buffer_ != NULL);
  if (buffer_ == NULL)
    return false;
  size_ = len;
  return true;
}

size_t FIFOChar::ReadAvailable() {
  return avail_;
}

size_t FIFOChar::WriteAvailable() {
  return size_ - avail_;
}

size_t FIFOChar::Peek(void* buf, size_t len) {
  char* ptr = static_cast<char*>(buf);

  size_t out = 0;
  len = std::min(len, avail_);

  size_t offs = tail_;
  while (len > 0) {
    size_t read_len = std::min(len, size_ - offs);
    memcpy(ptr, &buffer_[offs], read_len);

    ptr += read_len;
    offs += read_len;

    if (offs == size_)
      offs = 0;

    out += read_len;
    len -= read_len;
  }

  return out;
}

size_t FIFOChar::Read(void* buf, size_t len) {
  size_t read_len = Peek(buf, len);
  if (read_len > 0) {
    avail_ -= read_len;
    tail_ = (tail_ + read_len) % size_;
  }
  return read_len;
}

size_t FIFOChar::Write(const void* buf, size_t len) {
  const char* ptr = static_cast<const char*>(buf);
  size_t out = 0;
  size_t room = size_ - avail_;
  len = std::min(len, room);

  size_t offs = (tail_ + avail_) % size_;
  while (len > 0) {
    size_t write_len = std::min(len, size_ - offs);
    memcpy(&buffer_[offs], ptr, write_len);

    ptr += write_len;
    offs += write_len;
    if (offs == size_)
      offs = 0;

    out += write_len;
    len -= write_len;
  }

  avail_ += out;
  return out;
}

}  // namespace nacl_io
