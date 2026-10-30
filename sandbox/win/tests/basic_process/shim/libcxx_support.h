// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_LIBCXX_SUPPORT_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_LIBCXX_SUPPORT_H_

// Sources may also be compiled with the normal CRT, for example by unit tests.
// Leave libc++'s standard diagnostics in place in those builds.
#if defined(BASIC_PROCESS_NO_CRT)
#include <__config>

// Keep libc++ hardening checks without its out-of-line diagnostic runtime.
// This supported customization must precede all other libc++ headers.
#undef _LIBCPP_VERBOSE_ABORT
#define _LIBCPP_VERBOSE_ABORT(...) __builtin_trap()
#endif  // defined(BASIC_PROCESS_NO_CRT)

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_LIBCXX_SUPPORT_H_
