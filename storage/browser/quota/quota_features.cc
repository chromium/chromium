// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_features.h"

namespace storage {

namespace features {

// IncognitoDynamicQuota enables dynamic assignment of quota to incognito mode
// based on the physical memory size and removes the fixed upper cap for it.
const base::Feature kIncognitoDynamicQuota{"IncognitoDynamicQuota",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Dynamic quota for incognito mode would be set by a random fraction of
// physical memory, between |IncognitoQuotaRatioLowerBound| and
// |IncognitoQuotaRatioUpperBound|.
constexpr base::FeatureParam<double> kIncognitoQuotaRatioLowerBound{
    &kIncognitoDynamicQuota, "IncognitoQuotaRatioLowerBound", 0.15};
constexpr base::FeatureParam<double> kIncognitoQuotaRatioUpperBound{
    &kIncognitoDynamicQuota, "IncognitoQuotaRatioUpperBound", 0.2};

// Enables Storage Pressure Event.
const base::Feature kStoragePressureEvent{"StoragePressureEvent",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace storage
