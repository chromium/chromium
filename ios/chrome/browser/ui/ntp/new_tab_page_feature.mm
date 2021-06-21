// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Feature disabled by default to keep the legacy NTP until the refactored one
// covers all existing functionality.
const base::Feature kRefactoredNTP{"RefactoredNTP",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDiscoverFeedPreview{
    "EnableDiscoverFeedPreview", base::FEATURE_DISABLED_BY_DEFAULT};

const char kRefactoredNTPLoggingEnabled[] = "RefactoredNTPLoggingEnabled";

bool IsRefactoredNTP() {
  // This feature is dependent on the DiscoverFeed being enabled, only having
  // kRefactoredNTP enabled can lead to unexpected behavior.
  return base::FeatureList::IsEnabled(kRefactoredNTP) &&
         IsDiscoverFeedEnabled();
}

bool IsDiscoverFeedPreviewEnabled() {
  return base::FeatureList::IsEnabled(kEnableDiscoverFeedPreview);
}
