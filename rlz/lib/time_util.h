// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_TIME_UTIL_H_
#define RLZ_LIB_TIME_UTIL_H_

#include <stdint.h>

namespace rlz_lib {

// Returns the time relative to a fixed point in the past in multiples of 100 ns
// stepts. The point in the past is arbitrary but can't change, as the result of
// this value is stored on disk.
int64_t GetSystemTimeAsInt64();

}  // namespace rlz_lib

#endif  // RLZ_LIB_TIME_UTIL_H_
