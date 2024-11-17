// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/ppd.h>

// Function availability can be tested by checking whether its address is not
// nullptr. Weak symbols remove the need for platform specific build flags and
// allow for appropriate CUPS usage on platforms with non-uniform version
// support, namely Linux.
#define WEAK_CUPS_FN(x) extern "C" __attribute__((weak)) decltype(x) x

WEAK_CUPS_FN(httpConnect2);

// These may be removed when Amazon Linux 2 reaches EOL (30 Jun 2025).
WEAK_CUPS_FN(cupsFindDestDefault);
WEAK_CUPS_FN(cupsFindDestSupported);
WEAK_CUPS_FN(cupsUserAgent);
WEAK_CUPS_FN(ippValidateAttributes);
