// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "third_party/blink/common/common_export.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT extern const base::Feature kAutofillPreviewStyleExperiment;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kMojoBlobURLs;
BLINK_COMMON_EXPORT extern const base::Feature kNestedWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kOnionSoupDOMStorage;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature kRecordAnchorMetricsClicked;
BLINK_COMMON_EXPORT extern const base::Feature kRecordAnchorMetricsVisible;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerImportedScriptUpdateCheck;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerParallelSideDataReading;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerServicification;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kStopNonTimersInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kWritableFilesAPI;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kJankTracking;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;

BLINK_COMMON_EXPORT extern const char
    kAutofillPreviewStyleExperimentBgColorParameterName[];
BLINK_COMMON_EXPORT extern const char
    kAutofillPreviewStyleExperimentColorParameterName[];

BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeParamName[];
BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeBlockable[];
BLINK_COMMON_EXPORT extern const char
    kMixedContentAutoupgradeModeOptionallyBlockable[];

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
