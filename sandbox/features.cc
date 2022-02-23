// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/features.h"

namespace sandbox::features {
#if BUILDFLAG(IS_WIN)
bool IsAppContainerSandboxSupported() {
  // Since some APIs used for LPAC are unsupported below Windows 10 RS2 (1703
  // build 15063) so place a check here in a central place.
  static const bool supported =
      base::win::GetVersion() >= base::win::Version::WIN10_RS2;
  return supported;
}
#endif
}  // namespace sandbox::features