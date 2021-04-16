// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_TIME_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_TIME_H_

#include <time.h>

#if !defined(CPUCLOCK_CLOCK_MASK)
#define CPUCLOCK_CLOCK_MASK 3
#endif

#if !defined(CLOCKFD)
#define CLOCKFD 3
#endif

#if !defined(CLOCK_MONOTONIC_RAW)
#define CLOCK_MONOTONIC_RAW 4
#endif

#if !defined(CLOCK_REALTIME_COARSE)
#define CLOCK_REALTIME_COARSE 5
#endif

#if !defined(CLOCK_MONOTONIC_COARSE)
#define CLOCK_MONOTONIC_COARSE 6
#endif

#if !defined(CLOCK_BOOTTIME)
#define CLOCK_BOOTTIME 7
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_TIME_H_
