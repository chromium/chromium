// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_features.h"

#include "base/feature_list.h"

namespace content {

// When enabled, prefer to use the new recovery module to recover the
// `AttributionStorageSql` database. See https://crbug.com/1385500 for details.
// This is a kill switch and is not intended to be used in a field trial.
BASE_FEATURE(kAttributionStorageUseBuiltInRecoveryIfSupported,
             "AttributionStorageUseBuiltInRecoveryIfSupported",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionVerboseDebugReporting,
             "AttributionVerboseDebugReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace content
