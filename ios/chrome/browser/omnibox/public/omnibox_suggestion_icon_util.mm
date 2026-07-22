// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/public/omnibox_suggestion_icon_util.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

namespace {
const CGFloat kSymbolSize = 18;
}  // namespace

UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType icon_type) {
  Symbol symbol = SymbolGlobe;
  switch (icon_type) {
    case OmniboxSuggestionIconType::kCalculator:
      symbol = SymbolEqual;
      break;
    case OmniboxSuggestionIconType::kDefaultFavicon:
      symbol = SymbolGlobeAmericas;
      break;
    case OmniboxSuggestionIconType::kSearch:
      symbol = SymbolSearch;
      break;
    case OmniboxSuggestionIconType::kSearchHistory:
      symbol = SymbolHistory;
      break;
    case OmniboxSuggestionIconType::kConversion:
      symbol = SymbolSyncEnabled;
      break;
    case OmniboxSuggestionIconType::kDictionary:
      symbol = SymbolBookClosed;
      break;
    case OmniboxSuggestionIconType::kStock:
      symbol = SymbolSort;
      break;
    case OmniboxSuggestionIconType::kSunrise:
      symbol = SymbolSunFill;
      break;
    case OmniboxSuggestionIconType::kWhenIs:
      symbol = SymbolCalendar;
      break;
    case OmniboxSuggestionIconType::kTranslation:
      symbol = SymbolTranslate;
      break;
    case OmniboxSuggestionIconType::kFallbackAnswer:
      symbol = SymbolSearch;
      break;
    case OmniboxSuggestionIconType::kSearchTrend:
      symbol = SymbolUpTrend;
      break;
    case OmniboxSuggestionIconType::kSearchWithSparkle:
      symbol = SymbolMagnifyingglassSpark;
      break;
    case OmniboxSuggestionIconType::kNotesSpark:
      symbol = SymbolLineThreeSpark;
      break;
    case OmniboxSuggestionIconType::kCount:
      NOTREACHED();
  }

  return SymbolWithPointSize(symbol, kSymbolSize);
}

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
UIImage* GetBrandedGoogleIconForOmnibox() {
  return MakeSymbolMonochrome(
      SymbolWithPointSize(SymbolGoogleIcon, kSymbolSize));
}
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
