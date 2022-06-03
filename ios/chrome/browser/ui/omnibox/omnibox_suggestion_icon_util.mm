// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"

#include "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* GetOmniboxSuggestionIconTypeAssetName(
    OmniboxSuggestionIconType iconType) {
  switch (iconType) {
    case BOOKMARK:
      return @"omnibox_completion_bookmark";
    case CALCULATOR:
      return @"answer_calculator";
    case DEFAULT_FAVICON:
      return @"favicon_fallback";
    case HISTORY:
      return @"omnibox_completion_history";
    case SEARCH:
      return @"search";
    case CONVERSION:
      return @"answer_conversion";
    case DICTIONARY:
      return @"answer_dictionary";
    case STOCK:
      return @"answer_stock";
    case SUNRISE:
      return @"answer_sunrise";
    case LOCAL_TIME:
      return @"answer_local_time";
    case WHEN_IS:
      return @"answer_when_is";
    case TRANSLATION:
      return @"answer_translation";
    case FALLBACK_ANSWER:
      return @"search";
    case SEARCH_HISTORY:
      return @"omnibox_popup_recent_query";
    case OMNIBOX_SUGGESTION_ICON_TYPE_COUNT:
      NOTREACHED();
      return @"favicon_fallback";
  }
}

UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType iconType) {
  NSString* imageName = GetOmniboxSuggestionIconTypeAssetName(iconType);
  return [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}
