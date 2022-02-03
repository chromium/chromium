// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kReadingListMessagesOnlyJavaScriptExecutionParam[] =
    "javascript_only";

const base::Feature kReadingListMessages{"ReadingListMessages",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReadingListTimeToRead{"ReadingListTimeToRead",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsReadingListMessagesEnabled() {
  return base::FeatureList::IsEnabled(kReadingListMessages);
}

bool IsReadingListTimeToReadEnabled() {
  return base::FeatureList::IsEnabled(kReadingListTimeToRead);
}

bool ShouldNotPresentReadingListMessage() {
  return base::GetFieldTrialParamByFeatureAsBool(
             kReadingListMessages,
             kReadingListMessagesOnlyJavaScriptExecutionParam, false) ||
         IsReadingListTimeToReadEnabled();
}
