// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_SUGGESTION_ICON_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_SUGGESTION_ICON_UTIL_H_

#import <UIKit/UIKit.h>

// All available icons for autocomplete suggestions in the popup.
enum OmniboxSuggestionIconType {
  BOOKMARK = 0,
  CALCULATOR,
  DEFAULT_FAVICON,
  HISTORY,
  SEARCH,
  SEARCH_HISTORY,
  CONVERSION,
  DICTIONARY,
  STOCK,
  SUNRISE,
  LOCAL_TIME,
  WHEN_IS,
  TRANSLATION,
  // The FALLBACK_ANSWER icon is used for all answers that don't have special
  // icons above.
  FALLBACK_ANSWER,
  OMNIBOX_SUGGESTION_ICON_TYPE_COUNT,
};

// Returns the asset name (to be used in -[UIImage imageNamed:]).
NSString* GetOmniboxSuggestionIconTypeAssetName(OmniboxSuggestionIconType icon);

// Returns the asset with "always template" rendering mode.
UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType icon);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_SUGGESTION_ICON_UTIL_H_
