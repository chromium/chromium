// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_features.h"

namespace content::features {

BASE_FEATURE(kSharedStorageSelectURLLimit,
             "SharedStorageSelectURLLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kSharedStorageSelectURLBitBudgetPerPageLoad,
                   &kSharedStorageSelectURLLimit,
                   "SharedStorageSelectURLBitBudgetPerPageLoad",
                   12.0);
BASE_FEATURE_PARAM(double,
                   kSharedStorageSelectURLBitBudgetPerSitePerPageLoad,
                   &kSharedStorageSelectURLLimit,
                   "SharedStorageSelectURLBitBudgetPerSitePerPageLoad",
                   6.0);

}  // namespace content::features
