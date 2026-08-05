// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_WEAK_FUNCTIONS_H_
#define PRINTING_BACKEND_CUPS_WEAK_FUNCTIONS_H_

#include <cups/ppd.h>

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_LINUX));

// Function availability can be tested by checking whether its address is not
// nullptr. Weak symbols remove the need for platform specific build flags and
// allow for appropriate CUPS usage on platforms with non-uniform version
// support, namely Linux.
#define WEAK_CUPS_FN(x) extern "C" __attribute__((weak)) decltype(x) x

WEAK_CUPS_FN(httpConnect2);

#endif  // PRINTING_BACKEND_CUPS_WEAK_FUNCTIONS_H_
