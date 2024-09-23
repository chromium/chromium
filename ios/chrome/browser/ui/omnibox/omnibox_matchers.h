// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_MATCHERS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;
class GURL;

namespace omnibox {

/// Matcher for the primary text of OmniboxPopupRow.
id<GREYMatcher> PopupRowPrimaryTextMatcher();

/// Matcher for the secondary text of OmniboxPopupRow.
id<GREYMatcher> PopupRowSecondaryTextMatcher();

/// Matcher for the OmniboxPopupRow at index.
id<GREYMatcher> PopupRowAtIndex(NSIndexPath* index);

/// Matcher for OmniboxPopupRow with `url`.
id<GREYMatcher> PopupRowWithUrlMatcher(GURL url);

/// Matcher for the clear button in the omnibox.
id<GREYMatcher> ClearButtonMatcher();

}  // namespace omnibox

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_MATCHERS_H_
