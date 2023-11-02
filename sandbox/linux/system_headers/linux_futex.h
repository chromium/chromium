// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FUTEX_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FUTEX_H_

#include <linux/futex.h>

#if !defined(FUTEX_WAIT)
#define FUTEX_WAIT 0
#endif

#if !defined(FUTEX_WAKE)
#define FUTEX_WAKE 1
#endif

#if !defined(FUTEX_FD)
#define FUTEX_FD 2
#endif

#if !defined(FUTEX_REQUEUE)
#define FUTEX_REQUEUE 3
#endif

#if !defined(FUTEX_CMP_REQUEUE)
#define FUTEX_CMP_REQUEUE 4
#endif

#if !defined(FUTEX_WAKE_OP)
#define FUTEX_WAKE_OP 5
#endif

#if !defined(FUTEX_LOCK_PI)
#define FUTEX_LOCK_PI 6
#endif

#if !defined(FUTEX_UNLOCK_PI)
#define FUTEX_UNLOCK_PI 7
#endif

#if !defined(FUTEX_TRYLOCK_PI)
#define FUTEX_TRYLOCK_PI 8
#endif

#if !defined(FUTEX_WAIT_BITSET)
#define FUTEX_WAIT_BITSET 9
#endif

#if !defined(FUTEX_WAKE_BITSET)
#define FUTEX_WAKE_BITSET 10
#endif

#if !defined(FUTEX_WAIT_REQUEUE_PI)
#define FUTEX_WAIT_REQUEUE_PI 11
#endif

#if !defined(FUTEX_CMP_REQUEUE_PI)
#define FUTEX_CMP_REQUEUE_PI 12
#endif

#if !defined(FUTEX_PRIVATE_FLAG)
#define FUTEX_PRIVATE_FLAG 128
#endif

#if !defined FUTEX_CLOCK_REALTIME
#define FUTEX_CLOCK_REALTIME 256
#endif

#if !defined(FUTEX_CMD_MASK)
#define FUTEX_CMD_MASK ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)
#endif

#if !defined(FUTEX_CMP_REQUEUE_PI_PRIVATE)
#define FUTEX_CMP_REQUEUE_PI_PRIVATE (FUTEX_CMP_REQUEUE_PI | FUTEX_PRIVATE_FLAG)
#endif

#if !defined(FUTEX_UNLOCK_PI_PRIVATE)
#define FUTEX_UNLOCK_PI_PRIVATE (FUTEX_UNLOCK_PI | FUTEX_PRIVATE_FLAG)
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FUTEX_H_
