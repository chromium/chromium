// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_SDK_UTIL_STRING_UTIL_H_
#define LIBRARIES_SDK_UTIL_STRING_UTIL_H_

#include <string>
#include <vector>

namespace sdk_util {

// Splits |str| into a vector of strings delimited by |c|, placing the results
// in |r|. If several instances of |c| are contiguous, or if |str| begins with
// or ends with |c|, then an empty string is inserted. If |str| is empty, then
// no strings are inserted.
//
// NOTE: Does not trim white space.
inline void SplitString(const std::string& str,
                        char c,
                        std::vector<std::string>* r) {
  r->clear();
  size_t last = 0;
  size_t size = str.size();
  for (size_t i = 0; i <= size; ++i) {
    if (i == size || str[i] == c) {
      std::string tmp(str, last, i - last);
      // Avoid converting an empty source string into a vector of one empty
      // string.
      if (i != size || !r->empty() || !tmp.empty())
        r->push_back(tmp);
      last = i + 1;
    }
  }
}

}  // namespace sdk_util

#endif  // LIBRARIES_SDK_UTIL_STRING_UTIL_H_
