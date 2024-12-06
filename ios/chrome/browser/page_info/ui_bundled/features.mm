// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/page_info/core/features.h"

#import "ios/chrome/browser/page_info/ui_bundled/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

BASE_FEATURE(kPageInfoLastVisitedIOS,
             "PageInfoLastVisitedIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAboutThisSiteFeatureEnabled() {
  return page_info::IsAboutThisSiteFeatureEnabled(
      GetApplicationContext()->GetApplicationLocale());
}

bool IsPageInfoLastVisitedIOSEnabled() {
  return base::FeatureList::IsEnabled(kPageInfoLastVisitedIOS);
}
