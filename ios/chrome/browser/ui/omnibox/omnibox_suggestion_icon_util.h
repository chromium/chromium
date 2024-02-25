// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_SUGGESTION_ICON_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_SUGGESTION_ICON_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"

// All available icons for autocomplete suggestions in the popup.
enum class OmniboxSuggestionIconType {
  kCalculator = 0,
  kDefaultFavicon,
  kSearch,
  kSearchHistory,
  kConversion,
  kDictionary,
  kStock,
  kSunrise,
  kWhenIs,
  kTranslation,
  kSearchTrend,
  // The FALLBACK_ANSWER icon is used for all answers that don't have special
  // icons above.
  kFallbackAnswer,
  kCount,
};

// Returns the asset name (to be used in -[UIImage imageNamed:]).
NSString* GetOmniboxSuggestionIconTypeAssetName(OmniboxSuggestionIconType icon);

// Returns the asset with "always template" rendering mode.
UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType icon);

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
// Returns the branded Google icon.
UIImage* GetBrandedGoogleIconForOmnibox();
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_SUGGESTION_ICON_UTIL_H_
