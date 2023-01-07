// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"

#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* GetOmniboxSuggestionIconTypeAssetName(
    OmniboxSuggestionIconType iconType) {
  switch (iconType) {
    case OmniboxSuggestionIconType::kBookmark:
      return @"omnibox_completion_bookmark";
    case OmniboxSuggestionIconType::kCalculator:
      return @"answer_calculator";
    case OmniboxSuggestionIconType::kDefaultFavicon:
      return @"favicon_fallback";
    case OmniboxSuggestionIconType::kHistory:
      return @"omnibox_completion_history";
    case OmniboxSuggestionIconType::kSearch:
      return @"search";
    case OmniboxSuggestionIconType::kSearchHistory:
      return @"omnibox_popup_recent_query";
    case OmniboxSuggestionIconType::kConversation:
      return @"answer_conversion";
    case OmniboxSuggestionIconType::kDictionary:
      return @"answer_dictionary";
    case OmniboxSuggestionIconType::kStock:
      return @"answer_stock";
    case OmniboxSuggestionIconType::kSunrise:
      return @"answer_sunrise";
    case OmniboxSuggestionIconType::kLocalTime:
      return @"answer_local_time";
    case OmniboxSuggestionIconType::kWhenIs:
      return @"answer_when_is";
    case OmniboxSuggestionIconType::kTranslation:
      return @"answer_translation";
    case OmniboxSuggestionIconType::kFallbackAnswer:
      return @"search";
    case OmniboxSuggestionIconType::kCount:
      NOTREACHED();
      return @"favicon_fallback";
  }
}

UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType iconType) {
  NSString* imageName = GetOmniboxSuggestionIconTypeAssetName(iconType);
  return [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}
