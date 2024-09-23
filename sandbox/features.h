// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_FEATURES_H_
#define SANDBOX_FEATURES_H_

#include "build/build_config.h"

namespace sandbox::features {
#if BUILDFLAG(IS_WIN)
// Returns whether the App Container Sandbox is supported by the current
// Windows platform.
bool IsAppContainerSandboxSupported();
#endif
}  // namespace sandbox::features

#endif  // SANDBOX_FEATURES_H_
