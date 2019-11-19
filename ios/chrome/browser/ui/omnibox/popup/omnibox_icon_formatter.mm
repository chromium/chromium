// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_formatter.h"

#import "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

OmniboxSuggestionIconType IconTypeFromMatchAndAnswerType(
    AutocompleteMatchType::Type type,
    base::Optional<int> answerType) {
  // Some suggestions have custom icons. Others fallback to the icon from the
  // overall match type.
  if (answerType) {
    switch (answerType.value()) {
      case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
        return DICTIONARY;
      case SuggestionAnswer::ANSWER_TYPE_FINANCE:
        return STOCK;
      case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
        return TRANSLATION;
      case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
        return WHEN_IS;
      case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
        return CONVERSION;
      case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
        return SUNRISE;
      case SuggestionAnswer::ANSWER_TYPE_KNOWLEDGE_GRAPH:
      case SuggestionAnswer::ANSWER_TYPE_LOCAL:
      case SuggestionAnswer::ANSWER_TYPE_SPORTS:
      case SuggestionAnswer::ANSWER_TYPE_LOCAL_TIME:
      case SuggestionAnswer::ANSWER_TYPE_PLAY_INSTALL:
      case SuggestionAnswer::ANSWER_TYPE_WEATHER:
        return FALLBACK_ANSWER;
      case SuggestionAnswer::ANSWER_TYPE_INVALID:
      case SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT:
        NOTREACHED();
        break;
    }
  }
  switch (type) {
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
      return DEFAULT_FAVICON;
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::VOICE_SUGGEST:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return SEARCH;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return SEARCH_HISTORY;
    case AutocompleteMatchType::CALCULATOR:
      return CALCULATOR;
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      return DEFAULT_FAVICON;
  }
}

}  // namespace

@implementation OmniboxIconFormatter

- (instancetype)initWithMatch:(const AutocompleteMatch&)match {
  BOOL isAnswer = match.answer.has_value();
  OmniboxIconType iconType = OmniboxIconTypeSuggestionIcon;
  GURL imageURL = GURL();
  if (isAnswer && match.answer->second_line().image_url().is_valid()) {
    iconType = OmniboxIconTypeImage;
    imageURL = match.answer->second_line().image_url();
  } else if (!match.image_url.empty()) {
    iconType = OmniboxIconTypeImage;
    imageURL = GURL(match.image_url);
  } else if (!AutocompleteMatch::IsSearchType(match.type) &&
             !match.destination_url.is_empty()) {
    iconType = OmniboxIconTypeFavicon;
    imageURL = match.destination_url;
  } else {
    iconType = OmniboxIconTypeSuggestionIcon;
    imageURL = GURL();
  }

  auto answerType =
      isAnswer ? base::make_optional<int>(match.answer->type()) : base::nullopt;
  OmniboxSuggestionIconType suggestionIconType =
      IconTypeFromMatchAndAnswerType(match.type, answerType);
  return [self initWithIconType:iconType
             suggestionIconType:suggestionIconType
                       isAnswer:isAnswer
                       imageURL:imageURL];
}

@end
