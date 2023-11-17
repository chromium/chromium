// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace content::features {

// If enabled, then prefetch requests from speculation rules should use the code
// in content/browser/preloading/prefetch/ instead of
// chrome/browser/preloadingprefetch/prefetch_proxy/.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchUseContentRefactor);

// IF enabled, then redirects will be followed when prefetching.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchRedirects);

// If enabled, PrefetchContainer can be used for more than one navigation.
// https://crbug.com/1449360
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchReusable);

// The size limit of body size in bytes that can be reused in
// `kPrefetchReusable`.
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrefetchReusableBodySizeLimit;

// If enabled, navigational prefetch is scoped to the referring document's
// network isolation key instead of the old behavior of the referring document
// itself. See crbug.com/1502326
BASE_DECLARE_FEATURE(kPrefetchNIKScope);

// If enabled, the early cookie copy in `PrefetchDocumentManager` is
// skipped. See crbug.com/1503003 for details.
BASE_DECLARE_FEATURE(kPrefetchDocumentManagerEarlyCookieCopySkipped);

}  // namespace content::features

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
