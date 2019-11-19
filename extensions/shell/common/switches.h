// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SWITCHES_H_
#define EXTENSIONS_SHELL_COMMON_SWITCHES_H_

#include "build/build_config.h"

namespace extensions {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
#if defined(OS_CHROMEOS)
extern const char kAppShellAllowRoaming[];
extern const char kAppShellHostWindowSize[];
extern const char kAppShellPreferredNetwork[];
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kCrashDumpsDir[];
extern const char kEnableReporting[];
#endif

}  // namespace switches
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SWITCHES_H_
