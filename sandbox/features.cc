// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/features.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace sandbox::features {
#if BUILDFLAG(IS_WIN)
bool IsAppContainerSandboxSupported() {
  // Some APIs used for LPAC are unsupported below Windows 10 RS2 (1703 build
  // 15063). In addition, it is not possible to apply process mitigations to an
  // app container process until RS5. Place a check here in a central place.
  static const bool supported =
      base::win::GetVersion() >= base::win::Version::WIN10_RS5;
  return supported;
}
#endif
}  // namespace sandbox::features
