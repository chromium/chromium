// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ax_serialization_utils.h"

#include "build/build_config.h"

namespace content {

bool AXShouldIncludePageScaleFactorInRoot() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

}  // namespace content
