// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TEST_UTILS_H_
#define COMPONENTS_ZUCCHINI_TEST_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace zucchini {

// Parses space-separated list of byte hex values into list.
std::vector<uint8_t> ParseHexString(const std::string& hex_string);

// Returns a vector that's the contatenation of two vectors of the same type.
// Elements are copied by value.
template <class T>
std::vector<T> Cat(const std::vector<T>& a, const std::vector<T>& b) {
  std::vector<T> ret(a);
  ret.insert(ret.end(), b.begin(), b.end());
  return ret;
}

// Returns a subvector of a vector. Elements are copied by value.
template <class T>
std::vector<T> Sub(const std::vector<T>& a, size_t lo, size_t hi) {
  return std::vector<T>(a.begin() + lo, a.begin() + hi);
}

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TEST_UTILS_H_
