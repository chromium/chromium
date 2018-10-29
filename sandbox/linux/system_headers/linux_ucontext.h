// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_UCONTEXT_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_UCONTEXT_H_

#if defined(__native_client_nonsfi__)

#if defined(__arm__)
#include "sandbox/linux/system_headers/arm_linux_ucontext.h"
#elif defined(__i386__)
#include "sandbox/linux/system_headers/i386_linux_ucontext.h"
#else
#error "No support for your architecture in PNaCl header"
#endif

#else  // defined(__native_client_nonsfi__)
#error "The header file included on non PNaCl."
#endif  // defined(__native_client_nonsfi__)

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_UCONTEXT_H_
