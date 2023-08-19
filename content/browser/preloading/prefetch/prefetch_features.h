// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content::features {

// If enabled, then prefetch requests from speculation rules should use the code
// in content/browser/preloading/prefetch/ instead of
// chrome/browser/preloadingprefetch/prefetch_proxy/.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchUseContentRefactor);

// IF enabled, then redirects will be followed when prefetching.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchRedirects);

}  // namespace content::features

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
