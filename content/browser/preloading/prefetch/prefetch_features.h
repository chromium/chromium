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

// If enabled, then navigation requests should check the match responses in the
// prefetch cache by using the No-Vary-Search rules if No-Vary-Search header
// is specified in prefetched responses.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchNoVarySearch);

}  // namespace content::features

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
