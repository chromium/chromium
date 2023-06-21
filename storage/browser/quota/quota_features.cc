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

// Disables the `flush_to_media` setting for the quota SQLite db, which maps to
// F_FULLFSYNC on mac, which is known to have performance issues. Specifically,
// this is suspected of causing a high number of SQLite IO errors as encountered
// by CDM storage code.
BASE_FEATURE(kDisableQuotaDbFullFSync,
             "DisableQuotaDbFullFSync",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A kill switch for the new approach to storage eviction on low disk space. See
// crbug.com/1382847
BASE_FEATURE(kNewQuotaEvictionRoutine,
             "NewQuotaEvictionRoutine",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

}  // namespace features
}  // namespace storage
