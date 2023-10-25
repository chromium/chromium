// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/time_util.h"

#include "base/time/time.h"

namespace rlz_lib {

int64_t GetSystemTimeAsInt64() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds() * 10;
}

}  // namespace rlz_lib
