// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_REGION_H_
#define COURGETTE_REGION_H_

#include <stddef.h>
#include <stdint.h>

#include <string>


namespace courgette {

// A Region is a descriptor for a region of memory.  It has a start address and
// a length measured in bytes.  The Region object does not own the memory.
//
class Region {
 public:
  // Default constructor: and empty region.
  Region() : start_(nullptr), length_(0) {}

  // Usual constructor for regions given a |start| address and |length|.
  Region(const void* start, size_t length)
      : start_(static_cast<const uint8_t*>(start)), length_(length) {}

  // String constructor.  Region is owned by the string, so the string should
  // have a lifetime greater than the region.
  explicit Region(const std::string& string)
      : start_(reinterpret_cast<const uint8_t*>(string.c_str())),
        length_(string.length()) {}

  // Copy constructor.
  Region(const Region& other) : start_(other.start_), length_(other.length_) {}

  // Assignment 'operator' makes |this| region the same as |other|.
  Region& assign(const Region& other) {
    this->start_ = other.start_;
    this->length_ = other.length_;
    return *this;
  }

  // Returns the starting address of the region.
  const uint8_t* start() const { return start_; }

  // Returns the length of the region.
  size_t length() const { return length_; }

  // Returns the address after the last byte of the region.
  const uint8_t* end() const { return start_ + length_; }

 private:
  const uint8_t* start_;
  size_t length_;

  void operator=(const Region&);  // Disallow assignment operator.
};

}  // namespace
#endif  // COURGETTE_REGION_H_
