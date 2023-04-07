// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of symbol images.
const CGFloat kSymbolLocationBarPointSize = 10;

}  // namespace

#pragma mark - Suggestion icons.

OmniboxSuggestionIconType GetOmniboxSuggestionIconTypeForAutocompleteMatchType(
    AutocompleteMatchType::Type type) {
  // TODO(crbug.com/1122669): Handle trending zero-prefix suggestions by
  // checking the match subtype similar to AutocompleteMatch::GetVectorIcon().

  switch (type) {
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_CLUSTER:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::OPEN_TAB:
    case AutocompleteMatchType::PEDAL_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::STARTER_PACK:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return OmniboxSuggestionIconType::kDefaultFavicon;
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::VOICE_SUGGEST:
      return OmniboxSuggestionIconType::kSearch;
    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxSuggestionIconType::kSearchHistory;
    case AutocompleteMatchType::CALCULATOR:
      return OmniboxSuggestionIconType::kCalculator;
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::NULL_RESULT_MESSAGE:
    case AutocompleteMatchType::NUM_TYPES:
    case AutocompleteMatchType::TILE_SUGGESTION:
      NOTREACHED();
      return OmniboxSuggestionIconType::kDefaultFavicon;
  }
}

UIImage* GetOmniboxSuggestionIconForAutocompleteMatchType(
    AutocompleteMatchType::Type type) {
  OmniboxSuggestionIconType iconType =
      GetOmniboxSuggestionIconTypeForAutocompleteMatchType(type);
  return GetOmniboxSuggestionIcon(iconType);
}

#pragma mark - Security icons.

// Returns the asset with "always template" rendering mode.
UIImage* GetLocationBarSecurityIcon(LocationBarSecurityIconType iconType) {
  return DefaultSymbolTemplateWithPointSize(
      GetLocationBarSecuritySymbolName(iconType), kSymbolLocationBarPointSize);
}

// Converts the `security_level` to an appropriate security icon type.
LocationBarSecurityIconType GetLocationBarSecurityIconTypeForSecurityState(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
      return INFO;
    case security_state::DANGEROUS:
    case security_state::WARNING:
      return NOT_SECURE_WARNING;
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return SECURE;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return LOCATION_BAR_SECURITY_ICON_TYPE_COUNT;
  }
}

// Converts the `security_level` to an appropriate icon in "always template"
// rendering mode.
UIImage* GetLocationBarSecurityIconForSecurityState(
    security_state::SecurityLevel security_level) {
  LocationBarSecurityIconType iconType =
      GetLocationBarSecurityIconTypeForSecurityState(security_level);
  return GetLocationBarSecurityIcon(iconType);
}
