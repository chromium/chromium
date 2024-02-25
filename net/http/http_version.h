// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_VERSION_H_
#define NET_HTTP_HTTP_VERSION_H_

#include <stdint.h>

namespace net {

// Wrapper for an HTTP (major,minor) version pair.
// This type is final as the type is copy-constructable and assignable and so
// there is a risk of slicing if it was subclassed.
class HttpVersion final {
 public:
  // Default constructor (major=0, minor=0).
  constexpr HttpVersion() : value_(0) {}

  // Build from unsigned major/minor pair.
  constexpr HttpVersion(uint16_t major, uint16_t minor)
      : value_(static_cast<uint32_t>(major << 16) | minor) {}

  constexpr HttpVersion(const HttpVersion& rhs) = default;
  constexpr HttpVersion& operator=(const HttpVersion& rhs) = default;

  // Major version number.
  constexpr uint16_t major_value() const { return value_ >> 16; }

  // Minor version number.
  constexpr uint16_t minor_value() const { return value_ & 0xffff; }

  // Overloaded operators:

  constexpr bool operator==(const HttpVersion& v) const {
    return value_ == v.value_;
  }
  constexpr bool operator!=(const HttpVersion& v) const {
    return value_ != v.value_;
  }
  constexpr bool operator>(const HttpVersion& v) const {
    return value_ > v.value_;
  }
  constexpr bool operator>=(const HttpVersion& v) const {
    return value_ >= v.value_;
  }
  constexpr bool operator<(const HttpVersion& v) const {
    return value_ < v.value_;
  }
  constexpr bool operator<=(const HttpVersion& v) const {
    return value_ <= v.value_;
  }

 private:
  uint32_t value_;  // Packed as <major>:<minor>
};

}  // namespace net

#endif  // NET_HTTP_HTTP_VERSION_H_
