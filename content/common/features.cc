// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/features.h"

namespace content {

// Please keep features in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
CONSTINIT base::Feature kOnShowWithPageVisibility(
             "OnShowWithPageVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Please keep features in alphabetical order.

}  // namespace content