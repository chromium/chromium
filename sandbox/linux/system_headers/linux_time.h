// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_TIME_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_TIME_H_

#include <time.h>

#if !defined(CPUCLOCK_CLOCK_MASK)
#define CPUCLOCK_CLOCK_MASK 3
#endif

#if !defined(CPUCLOCK_PROF)
#define CPUCLOCK_PROF 0
#endif

#if !defined(CPUCLOCK_VIRT)
#define CPUCLOCK_VIRT 1
#endif

#if !defined(CPUCLOCK_SCHED)
#define CPUCLOCK_SCHED 2
#endif

#if !defined(CPUCLOCK_PERTHREAD_MASK)
#define CPUCLOCK_PERTHREAD_MASK 4
#endif

#if !defined(MAKE_PROCESS_CPUCLOCK)
#define MAKE_PROCESS_CPUCLOCK(pid, clock) \
  ((int)(~(unsigned)(pid) << 3) | (int)(clock))
#endif

#if !defined(MAKE_THREAD_CPUCLOCK)
#define MAKE_THREAD_CPUCLOCK(tid, clock) \
  ((int)(~(unsigned)(tid) << 3) | (int)((clock) | CPUCLOCK_PERTHREAD_MASK))
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
