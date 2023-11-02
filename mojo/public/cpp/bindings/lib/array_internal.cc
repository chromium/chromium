// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/array_internal.h"

#include <stddef.h>
#include <stdint.h>

#include <sstream>

namespace mojo {
namespace internal {

std::string MakeMessageWithArrayIndex(const char* message,
                                      size_t size,
                                      size_t index) {
  std::ostringstream stream;
  stream << message << ": array size - " << size << "; index - " << index;
  return stream.str();
}

std::string MakeMessageWithExpectedArraySize(const char* message,
                                             size_t size,
                                             size_t expected_size) {
  std::ostringstream stream;
  stream << message << ": array size - " << size << "; expected size - "
         << expected_size;
  return stream.str();
}

ArrayDataTraits<bool>::BitRef::~BitRef() {
}

ArrayDataTraits<bool>::BitRef::BitRef(uint8_t* storage, uint8_t mask)
    : storage_(storage), mask_(mask) {
}

ArrayDataTraits<bool>::BitRef& ArrayDataTraits<bool>::BitRef::operator=(
    bool value) {
  if (value) {
    *storage_ |= mask_;
  } else {
    *storage_ &= ~mask_;
  }
  return *this;
}

ArrayDataTraits<bool>::BitRef& ArrayDataTraits<bool>::BitRef::operator=(
    const BitRef& value) {
  return (*this) = static_cast<bool>(value);
}

ArrayDataTraits<bool>::BitRef::operator bool() const {
  return (*storage_ & mask_) != 0;
}

}  // namespace internal
}  // namespace mojo
