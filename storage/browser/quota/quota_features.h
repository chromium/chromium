// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace storage {

namespace features {

COMPONENT_EXPORT(STORAGE_BROWSER)
extern const base::Feature kStoragePressureEvent;

COMPONENT_EXPORT(STORAGE_BROWSER)
extern const base::Feature kStorageQuotaSettings;
extern const base::FeatureParam<double> kMustRemainAvailableBytes;
extern const base::FeatureParam<double> kMustRemainAvailableRatio;
extern const base::FeatureParam<double> kPoolSizeBytes;
extern const base::FeatureParam<double> kPoolSizeRatio;
extern const base::FeatureParam<double> kShouldRemainAvailableBytes;
extern const base::FeatureParam<double> kShouldRemainAvailableRatio;

COMPONENT_EXPORT(STORAGE_BROWSER)
extern const base::Feature kOnlySendStoragePolicyUpdatesForModifiedOrigins;

}  // namespace features

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_
