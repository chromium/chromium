// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_FEATURES_H_
#define SANDBOX_FEATURES_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace sandbox::features {
#if BUILDFLAG(IS_WIN)
// Returns whether the App Container Sandbox is supported by the current
// Windows platform.
bool IsAppContainerSandboxSupported();
#endif
}  // namespace sandbox::features

#endif
