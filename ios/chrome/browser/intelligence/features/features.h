// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_

#import "base/feature_list.h"

namespace base {
class TimeDelta;
}  // namespace base

// Feature flag controlling whether enhanced calendar is enabled.
BASE_DECLARE_FEATURE(kEnhancedCalendar);

// Returns true if enhanced calendar is enabled.
bool IsEnhancedCalendarEnabled();

// Feature flag controlling the page action menu.
BASE_DECLARE_FEATURE(kPageActionMenu);

// Returns true if the page action menu is enabled.
bool IsPageActionMenuEnabled();

// Feature flag controlling the cross-tab floaty chat persistence.
BASE_DECLARE_FEATURE(kGeminiCrossTab);

// Returns true if the cross-tab chat persistence is enabled for the floaty.
bool IsGeminiCrossTabEnabled();

// Whether the omnibox entry point opens the BWG overlay immediately, skipping
// the AI hub.
bool IsDirectBWGEntryPoint();
extern const char kPageActionMenuDirectEntryPointParam[];

// The BWG session validity duration in minutes.
const base::TimeDelta BWGSessionValidityDuration();
extern const char kBWGSessionValidityDurationParam[];

// Holds the variations of the BWG Promo Consent flow.
enum class BWGPromoConsentVariations {
  kDisabled = 0,
  kSinglePage = 1,
  kDoublePage = 2,
  kSkipConsent = 3,
  kForceFRE = 4,
};
extern const char kBWGPromoConsentParams[];

// Returns the variation of the BWG Promo Consent flow.
BWGPromoConsentVariations BWGPromoConsentVariationsParam();

// Returns YES if the promo should be forced.
bool ShouldForceBWGPromo();

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

// Feature flag to enable Precise Location in BWG Settings Menu.
BASE_DECLARE_FEATURE(kBWGPreciseLocation);

// Returns true if the precise location setting is enabled.
bool IsBWGPreciseLocationEnabled();

// Feature flag controlling the inclusion of anchor tags (links) in Page
// Context.
BASE_DECLARE_FEATURE(kPageContextAnchorTags);

// Returns true if the anchor tags are enabled in Page Context.
bool IsPageContextAnchorTagsEnabled();

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
