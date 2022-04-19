// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Feature disabled by default to keep showing old Zine feed.
const base::Feature kDiscoverFeedInNtp{"DiscoverFeedInNtp",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kSingleNtp{"SingleNTP", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kSingleCellContentSuggestions{
    "SingleCellContentSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kContentSuggestionsHeaderMigration{
    "ContentSuggestionsHeaderMigration", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kContentSuggestionsUIViewControllerMigration{
    "ContentSuggestionsUIViewControllerMigration",
    base::FEATURE_DISABLED_BY_DEFAULT};

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

bool IsDiscoverFeedEnabled() {
  return base::FeatureList::IsEnabled(kDiscoverFeedInNtp);
}

bool IsSingleNtpEnabled() {
  return base::FeatureList::IsEnabled(kSingleNtp);
}

bool IsSingleCellContentSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kSingleCellContentSuggestions);
}

bool IsContentSuggestionsHeaderMigrationEnabled() {
  return base::FeatureList::IsEnabled(kContentSuggestionsHeaderMigration);
}

bool IsContentSuggestionsUIViewControllerMigrationEnabled() {
  return base::FeatureList::IsEnabled(
      kContentSuggestionsUIViewControllerMigration);
}
