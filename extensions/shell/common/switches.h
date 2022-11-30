// Copyright 2014 The Chromium Authors
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
}  // namespace switches
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SWITCHES_H_
