// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/public/omnibox_util.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/omnibox/public/omnibox_icon_type.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_theme_resources.h"

namespace {

// The size of symbol images.
const CGFloat kSymbolLocationBarPointSize = 10;

// Returns the asset with "always template" rendering mode.
UIImage* GetLocationBarSecurityIcon(LocationBarSecurityIconType iconType) {
  Symbol symbol = GetLocationBarSecuritySymbol(iconType);
  if (symbol == SymbolNone) {
    return nil;
  }
  return SymbolTemplateWithPointSize(symbol, kSymbolLocationBarPointSize);
}

// Converts the `security_level` to an appropriate security icon type.
LocationBarSecurityIconType GetLocationBarSecurityIconTypeForSecurityState(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
      return LocationBarSecurityIconType::INFO;
    case security_state::DANGEROUS:
      return LocationBarSecurityIconType::DANGEROUS;
    case security_state::WARNING:
      return LocationBarSecurityIconType::NOT_SECURE_WARNING;
    case security_state::SECURE:
      return LocationBarSecurityIconType::NONE;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
  }
}

}  // namespace

#pragma mark - Suggestion icons.

OmniboxSuggestionIconType GetOmniboxSuggestionIconTypeForAutocompleteMatchType(
    omnibox::AutocompleteMatchType type) {
  switch (type) {
    case omnibox::AutocompleteMatchType::kBookmarkTitle:
    case omnibox::AutocompleteMatchType::kClipboardUrl:
    case omnibox::AutocompleteMatchType::kCrossDeviceTab:
    case omnibox::AutocompleteMatchType::kDocumentSuggestion:
    case omnibox::AutocompleteMatchType::kHistoryBody:
    case omnibox::AutocompleteMatchType::kHistoryCluster:
    case omnibox::AutocompleteMatchType::kHistoryKeyword:
    case omnibox::AutocompleteMatchType::kHistoryTitle:
    case omnibox::AutocompleteMatchType::kHistoryUrl:
    case omnibox::AutocompleteMatchType::kNavsuggest:
    case omnibox::AutocompleteMatchType::kNavsuggestPersonalized:
    case omnibox::AutocompleteMatchType::kOpenTab:
    case omnibox::AutocompleteMatchType::kPedal:
    case omnibox::AutocompleteMatchType::kPhysicalWebDeprecated:
    case omnibox::AutocompleteMatchType::kPhysicalWebOverflowDeprecated:
    case omnibox::AutocompleteMatchType::kStarterPack:
    case omnibox::AutocompleteMatchType::kTabSearchDeprecated:
    case omnibox::AutocompleteMatchType::kTileNavsuggest:
    case omnibox::AutocompleteMatchType::kTileMostVisitedSite:
    case omnibox::AutocompleteMatchType::kUrlWhatYouTyped:
      return OmniboxSuggestionIconType::kDefaultFavicon;
    case omnibox::AutocompleteMatchType::kClipboardImage:
    case omnibox::AutocompleteMatchType::kClipboardText:
    case omnibox::AutocompleteMatchType::kContactDeprecated:
    case omnibox::AutocompleteMatchType::kSearchOtherEngine:
    case omnibox::AutocompleteMatchType::kSearchSuggest:
    case omnibox::AutocompleteMatchType::kSearchSuggestEntity:
    case omnibox::AutocompleteMatchType::kSearchSuggestProfile:
    case omnibox::AutocompleteMatchType::kSearchSuggestTail:
    case omnibox::AutocompleteMatchType::kSearchWhatYouTyped:
    case omnibox::AutocompleteMatchType::kVoiceSuggest:
      return OmniboxSuggestionIconType::kSearch;
    case omnibox::AutocompleteMatchType::kSearchHistory:
    case omnibox::AutocompleteMatchType::kSearchSuggestPersonalized:
      return OmniboxSuggestionIconType::kSearchHistory;
    case omnibox::AutocompleteMatchType::kCalculator:
      return OmniboxSuggestionIconType::kCalculator;
    case omnibox::AutocompleteMatchType::kExtensionAppDeprecated:
    case omnibox::AutocompleteMatchType::kNullResultMessage:
    case omnibox::AutocompleteMatchType::kTileSuggestion:
    case omnibox::AutocompleteMatchType::kTileRepeatableQuery:
    case omnibox::AutocompleteMatchType::kHistoryEmbeddings:
    case omnibox::AutocompleteMatchType::kFeaturedEnterpriseSearch:
    case omnibox::AutocompleteMatchType::kHistoryEmbeddingsAnswer:
    default:
      DUMP_WILL_BE_NOTREACHED()
          << "Unsupported AutocompleteMatchType: " << static_cast<int>(type);
      return OmniboxSuggestionIconType::kDefaultFavicon;
  }
}

UIImage* GetOmniboxSuggestionIconForAutocompleteMatchType(
    omnibox::AutocompleteMatchType type) {
  OmniboxSuggestionIconType iconType =
      GetOmniboxSuggestionIconTypeForAutocompleteMatchType(type);
  return GetOmniboxSuggestionIcon(iconType);
}

OmniboxSuggestionIconType
GetOmniboxSuggestionIconTypeForSuggestTemplateInfoIconType(
    omnibox::SuggestTemplateInfo::IconType type) {
  // Update this assertion and the switch below whenever values are added.
  static_assert(omnibox::SuggestTemplateInfo::IconType_MAX ==
                omnibox::SuggestTemplateInfo::PROMPT_SUGGESTION);
  switch (type) {
    case omnibox::SuggestTemplateInfo_IconType_HISTORY:
      return OmniboxSuggestionIconType::kSearchHistory;
    case omnibox::SuggestTemplateInfo_IconType_SEARCH_LOOP:
      return OmniboxSuggestionIconType::kSearch;
    case omnibox::SuggestTemplateInfo_IconType_SEARCH_LOOP_WITH_SPARKLE:
      return OmniboxSuggestionIconType::kSearchWithSparkle;
    case omnibox::SuggestTemplateInfo_IconType_PROMPT_SUGGESTION:
      return OmniboxSuggestionIconType::kPromptSuggestion;
    case omnibox::SuggestTemplateInfo_IconType_TRENDING:
      return OmniboxSuggestionIconType::kSearchTrend;
    case omnibox::SuggestTemplateInfo_IconType_SUB_ARROW_RIGHT:
      // TODO(crbug.com/437177158): Replace with the correct symbol when it's
      // available.
      return OmniboxSuggestionIconType::kSearch;
    case omnibox::SuggestTemplateInfo_IconType_GLOBE_WITH_SEARCH_LOOP:
    case omnibox::SuggestTemplateInfo_IconType_BANANA:
      return OmniboxSuggestionIconType::kSearch;
    case omnibox::SuggestTemplateInfo_IconType_NOTES_SPARK:
      return OmniboxSuggestionIconType::kNotesSpark;
    case omnibox::SuggestTemplateInfo_IconType_DRAFT_SPARK:
    case omnibox::SuggestTemplateInfo_IconType_LIGHTBULB:
    case omnibox::SuggestTemplateInfo_IconType_ATTACH_FILE:
    case omnibox::SuggestTemplateInfo_IconType_SCHOOL:
    case omnibox::SuggestTemplateInfo_IconType_INK_PEN:
    case omnibox::SuggestTemplateInfo_IconType_TAB:
    case omnibox::SuggestTemplateInfo_IconType_PHOTO_SPARK:
    case omnibox::SuggestTemplateInfo_IconType_BOLT:
    case omnibox::SuggestTemplateInfo_IconType_IMAGE_CREATE:
      // TODO(crbug.com/486698515): Replace with the correct symbol when it's
      // available.
      return OmniboxSuggestionIconType::kSearch;
    case omnibox::SuggestTemplateInfo_IconType_FAVICON:
      return OmniboxSuggestionIconType::kDefaultFavicon;
    case omnibox::SuggestTemplateInfo_IconType_ICON_TYPE_UNSPECIFIED:
      return OmniboxSuggestionIconType::kSearch;
    default:
      return OmniboxSuggestionIconType::kSearch;
  }
}

UIImage* GetOmniboxSuggestionIconForSuggestTemplateInfoIconType(
    omnibox::SuggestTemplateInfo::IconType type) {
  OmniboxSuggestionIconType iconType =
      GetOmniboxSuggestionIconTypeForSuggestTemplateInfoIconType(type);
  return GetOmniboxSuggestionIcon(iconType);
}

#pragma mark - Security icons.

// Converts the `security_level` to an appropriate icon in "always template"
// rendering mode.
UIImage* GetLocationBarSecurityIconForSecurityState(
    security_state::SecurityLevel security_level) {
  LocationBarSecurityIconType iconType =
      GetLocationBarSecurityIconTypeForSecurityState(security_level);
  return GetLocationBarSecurityIcon(iconType);
}

UIImage* GetLocationBarOfflineIcon() {
  return SymbolTemplateWithPointSize(SymbolDownloadPromptFill,
                                     kSymbolLocationBarPointSize);
}
