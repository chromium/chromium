// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_features.h"
#include "base/feature_list.h"

namespace storage {

namespace features {

namespace {
constexpr int64_t kMBytes = 1024 * 1024;
}  // namespace

// Enables Storage Pressure Event.
BASE_FEATURE(kStoragePressureEvent,
             "StoragePressureEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables customized storage quota settings for embedders.
BASE_FEATURE(kStorageQuotaSettings,
             "StorageQuotaSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<double> kMustRemainAvailableBytes{
    &kStorageQuotaSettings, "MustRemainAvailableBytes", 1024 * kMBytes /* 1GB */
};
constexpr base::FeatureParam<double> kMustRemainAvailableRatio{
    &kStorageQuotaSettings, "MustRemainAvailableRatio", 0.01 /* 1% */
};
constexpr base::FeatureParam<double> kPoolSizeBytes{&kStorageQuotaSettings,
                                                    "PoolSizeBytes", 0};
constexpr base::FeatureParam<double> kPoolSizeRatio{
    &kStorageQuotaSettings, "PoolSizeRatio", 0.8 /* 80% */
};
constexpr base::FeatureParam<double> kShouldRemainAvailableBytes{
    &kStorageQuotaSettings, "ShouldRemainAvailableBytes",
    2048 * kMBytes /* 2GB */
};
constexpr base::FeatureParam<double> kShouldRemainAvailableRatio{
    &kStorageQuotaSettings, "ShouldRemainAvailableRatio", 0.1 /* 10% */
};

BASE_FEATURE(kEvictOrphanQuotaStorage,
             "EvictOrphanQuotaStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEvictStaleQuotaStorage,
             "EvictStaleQuotaStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace storage
