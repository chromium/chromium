// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_formatter.h"

#import "base/notreached.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

OmniboxSuggestionIconType IconTypeFromMatchAndAnswerType(
    AutocompleteMatchType::Type type,
    absl::optional<int> answerType) {
  // Some suggestions have custom icons. Others fallback to the icon from the
  // overall match type.
  if (answerType) {
    switch (answerType.value()) {
      case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
        return OmniboxSuggestionIconType::kDictionary;
      case SuggestionAnswer::ANSWER_TYPE_FINANCE:
        return OmniboxSuggestionIconType::kStock;
      case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
        return OmniboxSuggestionIconType::kTranslation;
      case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
        return OmniboxSuggestionIconType::kWhenIs;
      case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
        return OmniboxSuggestionIconType::kConversion;
      case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
        return OmniboxSuggestionIconType::kSunrise;
      case SuggestionAnswer::ANSWER_TYPE_KNOWLEDGE_GRAPH:
      case SuggestionAnswer::ANSWER_TYPE_LOCAL:
      case SuggestionAnswer::ANSWER_TYPE_LOCAL_TIME:
      case SuggestionAnswer::ANSWER_TYPE_PLAY_INSTALL:
      case SuggestionAnswer::ANSWER_TYPE_SPORTS:
      case SuggestionAnswer::ANSWER_TYPE_WEATHER:
        return OmniboxSuggestionIconType::kFallbackAnswer;
      case SuggestionAnswer::ANSWER_TYPE_INVALID:
      case SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT:
        NOTREACHED();
        break;
    }
  }
  return GetOmniboxSuggestionIconTypeForAutocompleteMatchType(type);
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
  } else if (!match.image_url.is_empty()) {
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
      isAnswer ? absl::make_optional<int>(match.answer->type()) : absl::nullopt;
  OmniboxSuggestionIconType suggestionIconType =
      IconTypeFromMatchAndAnswerType(match.type, answerType);
  return [self initWithIconType:iconType
             suggestionIconType:suggestionIconType
                       isAnswer:isAnswer
                       imageURL:[[CrURL alloc] initWithGURL:imageURL]];
}

@end
