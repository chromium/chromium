// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/bit_reader.h"

namespace media {

BitReader::BitReader(const uint8_t* data, int size)
    : initial_size_(size),
      data_(data),
      bytes_left_(size),
      bit_reader_core_(this) {
  DCHECK(data != NULL);
  DCHECK_GE(size, 0);
}

BitReader::~BitReader() = default;

bool BitReader::ReadString(int num_bits, std::string* str) {
  DCHECK_EQ(num_bits % 8, 0);
  DCHECK_GT(num_bits, 0);
  DCHECK(str);
  int num_bytes = num_bits / 8;
  str->resize(num_bytes);
  char* ptr = &str->front();
  while (num_bytes--) {
    if (!ReadBits(8, ptr++))
      return false;
  }
  return true;
}

int BitReader::GetBytes(int max_nbytes, const uint8_t** out) {
  DCHECK_GE(max_nbytes, 0);
  DCHECK(out);

  int nbytes = max_nbytes;
  if (nbytes > bytes_left_)
    nbytes = bytes_left_;

  *out = data_;
  data_ += nbytes;
  bytes_left_ -= nbytes;
  return nbytes;
}

}  // namespace media
