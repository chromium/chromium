// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_

#import "base/feature_list.h"

// Feature flag controlling whether enhanced calendar is enabled.
BASE_DECLARE_FEATURE(kEnhancedCalendar);

// Returns true if enhanced calendar is enabled.
bool IsEnhancedCalendarEnabled();

// Feature flag controlling the page action menu.
BASE_DECLARE_FEATURE(kPageActionMenu);

// Returns true if the page action menu is enabled.
bool IsPageActionMenuEnabled();

// Whether the omnibox entry point opens the BWG overlay immediately, skipping
// the AI hub.
bool IsDirectBWGEntryPoint();
extern const char kPageActionMenuDirectEntryPointParam[];

// Holds the variations of the BWG Promo Consent flow.
enum class BWGPromoConsentVariations {
  kDisabled = 0,
  kSinglePage = 1,
  kDoublePage = 2,
  kSkipConsent = 3,
  kForceConsent = 4,
};
extern const char kBWGPromoConsentParams[];

// Returns the variation of the BWG Promo Consent flow.
BWGPromoConsentVariations BWGPromoConsentVariationsParam();

// Feature flag to enable BWG Promo Consent.
BASE_DECLARE_FEATURE(kBWGPromoConsent);

extern const char kExplainGeminiEditMenuParams[];

// Holds the position of Explain Gemini button in the EditMenu.
enum class PositionForExplainGeminiEditMenu {
  kDisabled = 0,
  kAfterEdit = 1,
  kAfterSearch = 2,
};

// Returns the position of Explain Gemini in the EditMenu.
PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition();

// Feature flag to enable Explain Gemini in Edit Menu.
BASE_DECLARE_FEATURE(kExplainGeminiEditMenu);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
