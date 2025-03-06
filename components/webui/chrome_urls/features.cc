// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui/chrome_urls/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace chrome_urls {

BASE_FEATURE(kInternalOnlyUisPref,
             "InternalOnlyUisPref",
#if !BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace chrome_urls
