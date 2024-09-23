// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/page_info/core/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/ui/page_info/features.h"

BASE_FEATURE(kRevampPageInfoIos,
             "RevampPageInfoIos",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageInfoLastVisitedIOS,
             "kPageInfoLastVisitedIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRevampPageInfoIosEnabled() {
  return base::FeatureList::IsEnabled(kRevampPageInfoIos);
}

bool IsAboutThisSiteFeatureEnabled() {
  return IsRevampPageInfoIosEnabled() &&
         page_info::IsAboutThisSiteFeatureEnabled(
             GetApplicationContext()->GetApplicationLocale());
}

bool IsPageInfoLastVisitedIOSEnabled() {
  return base::FeatureList::IsEnabled(kPageInfoLastVisitedIOS);
}
