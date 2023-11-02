// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_TEST_BYTE_VECTOR_UTILS_H_
#define REMOTING_HOST_FILE_TRANSFER_TEST_BYTE_VECTOR_UTILS_H_

#include <cstdint>
#include <vector>

namespace remoting {

// Base case for AppendBytes template.
inline void AppendBytes(std::vector<uint8_t>& vec) {}

// Appends the bytes from each of the passed iterables to |vec|.
template <typename Iterable, typename... Iterables>
void AppendBytes(std::vector<uint8_t>& vec,
                 const Iterable& first,
                 const Iterables&... rest) {
  using std::begin;
  using std::end;
  vec.insert(vec.end(), begin(first), end(first));
  AppendBytes(vec, rest...);
}

// Creates a byte array from the provided arguments. The arguments must be
// iterable using begin() and end(). The returned vector will contain the
// concatenation of the bytes yielded by each iterable.
template <typename... Iterables>
std::vector<std::uint8_t> ByteArrayFrom(const Iterables&... iterables) {
  std::vector<std::uint8_t> vec;
  AppendBytes(vec, iterables...);
  return vec;
}

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_TEST_BYTE_VECTOR_UTILS_H_
