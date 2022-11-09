// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/io_utils.h"

#include <iostream>

namespace zucchini {

/******** LimitedOutputStream::StreamBuf ********/

LimitedOutputStream::StreamBuf::StreamBuf(std::ostream& os, int limit)
    : os_(os), limit_(limit) {}

LimitedOutputStream::StreamBuf::~StreamBuf() {
  // Display warning in case we forget to flush data with std::endl.
  if (!str().empty()) {
    std::cerr << "Warning: LimitedOutputStream has " << str().length()
              << " bytes of unflushed output." << std::endl;
  }
}

int LimitedOutputStream::StreamBuf::sync() {
  if (full()) {
    str("");
    return 0;
  }
  *os_ << str();
  str("");
  if (++counter_ >= limit_)
    *os_ << "(Additional output suppressed)\n";
  os_->flush();
  return 0;
}

/******** LimitedOutputStream ********/

LimitedOutputStream::LimitedOutputStream(std::ostream& os, int limit)
    : std::ostream(&buf_), buf_(os, limit) {}

/******** PrefixSep ********/

std::ostream& operator<<(std::ostream& ostr, PrefixSep& obj) {
  if (obj.first_)
    obj.first_ = false;
  else
    ostr << obj.sep_str_;
  return ostr;
}

}  // namespace zucchini
