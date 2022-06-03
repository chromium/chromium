// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_MACROS_H_
#define HEADLESS_LIB_HEADLESS_MACROS_H_

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MAC)
#define HEADLESS_USE_BREAKPAD
#endif  // defined(OS_POSIX) && !defined(OS_MAC)

#endif  // HEADLESS_LIB_HEADLESS_MACROS_H_
