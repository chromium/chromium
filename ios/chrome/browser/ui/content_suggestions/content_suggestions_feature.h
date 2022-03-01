// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose between the old Zine feed or the new Discover feed in the
// Bling new tab page.
// Use IsDiscoverFeedEnabled() instead of this constant directly.
extern const base::Feature kDiscoverFeedInNtp;

// Feature to use one NTP for all tabs in a Browser.
extern const base::Feature kSingleNtp;

// Feature to use one cell for Content Suggestions
extern const base::Feature kSingleCellContentSuggestions;

// Feature to move the Content Suggestions header view into the Discover
// ScrollView.
extern const base::Feature kContentSuggestionsHeaderMigration;

// Feature to move the Content Suggestions ViewController to a UIViewController.
extern const base::Feature kContentSuggestionsUIViewControllerMigration;

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Whether the Discover feed is enabled instead of the Zine feed.
bool IsDiscoverFeedEnabled();

// Whether the single ntp feature is enabled.
bool IsSingleNtpEnabled();

// Whether the single cell content suggestions feature is enabled.
bool IsSingleCellContentSuggestionsEnabled();

// Whether the Content Suggestions header migration feature is enabled.
bool IsContentSuggestionsHeaderMigrationEnabled();

// Whether the Content Suggestions UIViewController migration feature is
// enabled.
bool IsContentSuggestionsUIViewControllerMigrationEnabled();

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_
