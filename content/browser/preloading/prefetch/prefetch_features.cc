// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_features.h"

namespace content::features {

BASE_FEATURE(kPrefetchUseContentRefactor,
             "PrefetchUseContentRefactor",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

}  // namespace content::features
