// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_TEST_UTIL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_TEST_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/callback.h"

namespace base {
class FilePath;
}

namespace disk_cache {
namespace simple_util {

// Immutable array with compile-time bound-checking.
template <typename T, size_t Size>
class ImmutableArray {
 public:
  static const size_t size = Size;

  ImmutableArray(const base::RepeatingCallback<T(size_t index)>& initializer) {
    for (size_t i = 0; i < size; ++i)
      data_[i] = initializer.Run(i);
  }

  template <size_t Index>
  const T& at() const {
    static_assert(Index < size, "array out of bounds");
    return data_[Index];
  }

 private:
  T data_[size];
};

// Creates a corrupt file to be used in tests.
bool CreateCorruptFileForTests(const std::string& key,
                               const base::FilePath& cache_path);

// Removes the key SHA256 from an entry.
bool RemoveKeySHA256FromEntry(const std::string& key,
                              const base::FilePath& cache_path);

// Modifies the key SHA256 from an entry so that it is corrupt.
bool CorruptKeySHA256FromEntry(const std::string& key,
                               const base::FilePath& cache_path);

// Modifies the stream 0 length field from an entry so it is invalid.
bool CorruptStream0LengthFromEntry(const std::string& key,
                                   const base::FilePath& cache_path);

}  // namespace simple_util
}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_TEST_UTIL_H_
