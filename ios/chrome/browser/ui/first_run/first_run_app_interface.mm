// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"

#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation FirstRunAppInterface

+ (BOOL)isUMACollectionEnabled {
  return GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
}

+ (BOOL)isOmniboxPositionChoiceEnabled {
  return IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kFRE) &&
         ShouldShowOmniboxPositionChoiceInFRE(
             chrome_test_util::GetOriginalBrowserState());
}

@end
