// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

namespace {
const CGFloat kSymbolSize = 18;
}  // namespace

UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType icon_type) {
  NSString* symbol_name = kGlobeSymbol;
  bool default_symbol = true;
  switch (icon_type) {
    case OmniboxSuggestionIconType::kCalculator:
      symbol_name = kEqualSymbol;
      break;
    case OmniboxSuggestionIconType::kDefaultFavicon:
      symbol_name = kGlobeAmericasSymbol;
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
    case OmniboxSuggestionIconType::kSearchTrend:
      symbol_name = kUpTrendSymbol;
      default_symbol = false;
      break;
    case OmniboxSuggestionIconType::kCount:
      NOTREACHED_IN_MIGRATION();
      symbol_name = kGlobeAmericasSymbol;
      break;
  }

  if (default_symbol) {
    return DefaultSymbolWithPointSize(symbol_name, kSymbolSize);
  }
  return CustomSymbolWithPointSize(symbol_name, kSymbolSize);
}

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
UIImage* GetBrandedGoogleIconForOmnibox() {
  return MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolSize));
}
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
