// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_FEATURES_H_
#define CONTENT_BROWSER_SHARED_STORAGE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace content::features {

// If enabled, limits the number of times per origin per pageload that
// `sharedStorage.selectURL()` is allowed to be invoked.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedStorageSelectURLLimit);
// Maximum number of bits of entropy per pageload that are allowed to leak via
// `sharedStorage.selectURL()`, if `kSharedStorageSelectURLLimit` is enabled.
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kSharedStorageSelectURLBitBudgetPerPageLoad);
// Maximum number of bits of entropy per site per pageload that are allowed to
// leak via `sharedStorage.selectURL()`, if `kSharedStorageSelectURLLimit` is
// enabled.
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kSharedStorageSelectURLBitBudgetPerSitePerPageLoad);

}  // namespace content::features

#endif  // CONTENT_BROWSER_SHARED_STORAGE_FEATURES_H_
