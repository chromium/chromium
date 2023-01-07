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

#if BUILDFLAG(IS_ANDROID)

// https://android.googlesource.com/platform/bionic/+/lollipop-release/libc/private/bionic_prctl.h
#if !defined(PR_SET_VMA)
#define PR_SET_VMA 0x53564d41
#endif

#endif  // BUILDFLAG(IS_ANDROID)

#if !defined(PR_SET_PTRACER)
#define PR_SET_PTRACER 0x59616d61
#endif

#if !defined(PR_SET_PTRACER_ANY)
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_PRCTL_H_
