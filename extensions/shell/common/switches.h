// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SWITCHES_H_
#define EXTENSIONS_SHELL_COMMON_SWITCHES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace extensions {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kAppShellAllowRoaming[];
extern const char kAppShellHostWindowSize[];
extern const char kAppShellPreferredNetwork[];
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kCrashDumpsDir[];
extern const char kEnableReporting[];
#endif

}  // namespace switches
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SWITCHES_H_
