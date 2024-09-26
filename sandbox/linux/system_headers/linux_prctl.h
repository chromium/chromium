// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_PRCTL_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_PRCTL_H_

#include "build/build_config.h"

#if !defined(PR_SET_PDEATHSIG)
#define PR_SET_PDEATHSIG 1
#endif

#if !defined(PR_SET_TIMERSLACK)
#define PR_SET_TIMERSLACK 29
#endif

// The PR_SET_VMA* symbols are originally from
// https://android.googlesource.com/platform/bionic/+/lollipop-release/libc/private/bionic_prctl.h
// and were subsequently added to mainline Linux in Jan 2022.
//
// We conditionally define these symbols here to support older
// GNU/Linux operating systems that may not have these symbols yet.
#if !defined(PR_SET_VMA)
#define PR_SET_VMA 0x53564d41
#endif

#if !defined(PR_SET_VMA_ANON_NAME)
#define PR_SET_VMA_ANON_NAME 0
#endif

#if !defined(PR_SET_PTRACER)
#define PR_SET_PTRACER 0x59616d61
#endif

#if !defined(PR_SET_PTRACER_ANY)
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#if !defined(PR_SVE_GET_VL)
#define PR_SVE_GET_VL 51
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_PRCTL_H_
