// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_TEST_UTILS_H_

#import <UIKit/UIKit.h>

#include "url/gurl.h"

@protocol GREYMatcher;
@class SetUpListItemView;

namespace ntp_home {
// Returns the parent view containing all NTP content. Returns nil if it is not
// in the view hierarchy.
UIView* NTPView();

// Returns the primary collection view of the new tab page. Returns nil if it is
// not in the view hierarchy.
UICollectionView* CollectionView();

// Returns the collection view of the content suggestions. Returns nil if it is
// not in the view hierarchy.
UICollectionView* ContentSuggestionsCollectionView();

// Returns the view corresponding to the fake omnibox. Returns nil if it is not
// in the view hierarchy.
UIView* FakeOmnibox();

// Returns the label corresponding to the Discover header label. Returns nil if
// it is not in the view hierarchy.
UILabel* DiscoverHeaderLabel();

// Returns the SetUpListItemView in a Magic Stack module with the given
// `accessibility_id`.
SetUpListItemView* SetUpListItemViewInMagicStackWithAccessibilityId(
    NSString* accessibility_id);

}  // namespace ntp_home

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_TEST_UTILS_H_
