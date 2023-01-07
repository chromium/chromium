// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/time_util.h"

#include <windows.h>

namespace rlz_lib {

int64_t GetSystemTimeAsInt64() {
  FILETIME now_as_file_time;
  // Relative to Jan 1, 1601 (UTC).
  ::GetSystemTimeAsFileTime(&now_as_file_time);

  LARGE_INTEGER integer;
  integer.HighPart = now_as_file_time.dwHighDateTime;
  integer.LowPart = now_as_file_time.dwLowDateTime;
  return integer.QuadPart;
}

}  // namespace rlz_lib
