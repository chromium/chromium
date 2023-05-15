// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/der/input.h"

#include <algorithm>

#include "base/check_op.h"

namespace net::der {

Input::Input(std::string_view in)
    : data_(reinterpret_cast<const uint8_t*>(in.data())), len_(in.length()) {}

Input::Input(const std::string* s) : Input(std::string_view(*s)) {}

std::string Input::AsString() const {
  return std::string(reinterpret_cast<const char*>(data_), len_);
}

std::string_view Input::AsStringView() const {
  return std::string_view(reinterpret_cast<const char*>(data_), len_);
}

base::span<const uint8_t> Input::AsSpan() const {
  return base::make_span(data_, len_);
}

bool operator==(const Input& lhs, const Input& rhs) {
  return lhs.Length() == rhs.Length() &&
         std::equal(lhs.UnsafeData(), lhs.UnsafeData() + lhs.Length(),
                    rhs.UnsafeData());
}

bool operator!=(const Input& lhs, const Input& rhs) {
  return !(lhs == rhs);
}

ByteReader::ByteReader(const Input& in)
    : data_(in.UnsafeData()), len_(in.Length()) {
}

bool ByteReader::ReadByte(uint8_t* byte_p) {
  if (!HasMore())
    return false;
  *byte_p = *data_;
  Advance(1);
  return true;
}

bool ByteReader::ReadBytes(size_t len, Input* out) {
  if (len > len_)
    return false;
  *out = Input(data_, len);
  Advance(len);
  return true;
}

// Returns whether there is any more data to be read.
bool ByteReader::HasMore() {
  return len_ > 0;
}

void ByteReader::Advance(size_t len) {
  CHECK_LE(len, len_);
  data_ += len;
  len_ -= len;
}

}  // namespace net::der
