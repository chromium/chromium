// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

void RecordAIHubAction(IOSAIHubAction action) {
  base::UmaHistogramEnumeration("IOS.AIHub.Action", action);
}

void RecordPageActionMenuFeatureRowShown(
    IOSPageActionMenuFeatureType feature_type) {
  base::UmaHistogramEnumeration("IOS.PageActionMenu.FeatureRowShown",
                                feature_type);
}

void RecordPageActionMenuFeatureRowUsed(
    IOSPageActionMenuFeatureType feature_type) {
  base::UmaHistogramEnumeration("IOS.PageActionMenu.FeatureRowUsed",
                                feature_type);
}

void RecordPageActionMenuFeatureRowSettingsOpened(
    IOSPageActionMenuFeatureType feature_type) {
  base::UmaHistogramEnumeration("IOS.PageActionMenu.FeatureRowSettingsOpened",
                                feature_type);
}
