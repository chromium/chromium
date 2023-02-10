// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/icons/symbols.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSymbolSize = 18;
}  // namespace

NSString* GetOmniboxSuggestionIconTypeAssetName(
    OmniboxSuggestionIconType icon_type) {
  DCHECK(!UseSymbolsInOmnibox());
  switch (icon_type) {
    case OmniboxSuggestionIconType::kCalculator:
      return @"answer_calculator";
    case OmniboxSuggestionIconType::kDefaultFavicon:
      return @"favicon_fallback";
    case OmniboxSuggestionIconType::kSearch:
      return @"search";
    case OmniboxSuggestionIconType::kSearchHistory:
      return @"omnibox_popup_recent_query";
    case OmniboxSuggestionIconType::kConversion:
      return @"answer_conversion";
    case OmniboxSuggestionIconType::kDictionary:
      return @"answer_dictionary";
    case OmniboxSuggestionIconType::kStock:
      return @"answer_stock";
    case OmniboxSuggestionIconType::kSunrise:
      return @"answer_sunrise";
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

UIImage* GetOmniboxSuggestionSymbol(OmniboxSuggestionIconType icon_type) {
  NSString* symbol_name = kGlobeSymbol;
  bool default_symbol = true;
  switch (icon_type) {
    case OmniboxSuggestionIconType::kCalculator:
      symbol_name = kEqualSymbol;
      break;
    case OmniboxSuggestionIconType::kDefaultFavicon:
      if (@available(iOS 15, *)) {
        symbol_name = kGlobeAmericasSymbol;
      } else {
        symbol_name = kGlobeSymbol;
      }
      break;
    case OmniboxSuggestionIconType::kSearch:
      symbol_name = kSearchSymbol;
      break;
    case OmniboxSuggestionIconType::kSearchHistory:
      symbol_name = kHistorySymbol;
      break;
    case OmniboxSuggestionIconType::kConversion:
      symbol_name = kSyncEnabledSymbol;
      break;
    case OmniboxSuggestionIconType::kDictionary:
      symbol_name = kBookClosedSymbol;
      break;
    case OmniboxSuggestionIconType::kStock:
      symbol_name = kSortSymbol;
      break;
    case OmniboxSuggestionIconType::kSunrise:
      symbol_name = kSunFillSymbol;
      break;
    case OmniboxSuggestionIconType::kWhenIs:
      symbol_name = kCalendarSymbol;
      break;
    case OmniboxSuggestionIconType::kTranslation:
      symbol_name = kTranslateSymbol;
      default_symbol = false;
      break;
    case OmniboxSuggestionIconType::kFallbackAnswer:
      symbol_name = kSearchSymbol;
      break;
    case OmniboxSuggestionIconType::kCount:
      NOTREACHED();
      if (@available(iOS 15, *)) {
        symbol_name = kGlobeAmericasSymbol;
      } else {
        symbol_name = kGlobeSymbol;
      }
      break;
  }

  if (default_symbol) {
    return DefaultSymbolWithPointSize(symbol_name, kSymbolSize);
  }
  return CustomSymbolWithPointSize(symbol_name, kSymbolSize);
}

UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType icon_type) {
  if (UseSymbolsInOmnibox()) {
    return GetOmniboxSuggestionSymbol(icon_type);
  }

  NSString* imageName = GetOmniboxSuggestionIconTypeAssetName(icon_type);
  return [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
UIImage* GetBrandedGoogleIcon() {
  DCHECK(UseSymbolsInOmnibox());
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolSize));
}
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
