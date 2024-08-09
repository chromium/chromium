// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_formatter.h"

#import "base/notreached.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_feature_configs.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "url/gurl.h"

namespace {

OmniboxSuggestionIconType IconTypeFromMatch(const AutocompleteMatch& match) {
  // Some suggestions have custom icons. Others fallback to the icon from the
  // overall match type.
  omnibox::AnswerType answer_type = match.answer_type;
  if (answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
    switch (answer_type) {
      case omnibox::ANSWER_TYPE_DICTIONARY:
        return OmniboxSuggestionIconType::kDictionary;
      case omnibox::ANSWER_TYPE_FINANCE:
        return OmniboxSuggestionIconType::kStock;
      case omnibox::ANSWER_TYPE_TRANSLATION:
        return OmniboxSuggestionIconType::kTranslation;
      case omnibox::ANSWER_TYPE_WHEN_IS:
        return OmniboxSuggestionIconType::kWhenIs;
      case omnibox::ANSWER_TYPE_CURRENCY:
        return OmniboxSuggestionIconType::kConversion;
      case omnibox::ANSWER_TYPE_SUNRISE_SUNSET:
        return OmniboxSuggestionIconType::kSunrise;
      case omnibox::ANSWER_TYPE_GENERIC_ANSWER:
      case omnibox::ANSWER_TYPE_LOCAL_TIME:
      case omnibox::ANSWER_TYPE_PLAY_INSTALL:
      case omnibox::ANSWER_TYPE_SPORTS:
      case omnibox::ANSWER_TYPE_WEATHER:
      case omnibox::ANSWER_TYPE_WEB_ANSWER:
        return OmniboxSuggestionIconType::kFallbackAnswer;
      case omnibox::ANSWER_TYPE_UNSPECIFIED:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  if (match.IsTrendSuggestion()) {
    return OmniboxSuggestionIconType::kSearchTrend;
  }

  return GetOmniboxSuggestionIconTypeForAutocompleteMatchType(match.type);
}

}  // namespace

@implementation OmniboxIconFormatter

- (instancetype)initWithMatch:(const AutocompleteMatch&)match {
  BOOL suggestionAnswerMigrationEnabled =
      omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled;
  BOOL isAnswer = suggestionAnswerMigrationEnabled
                      ? match.answer_template.has_value()
                      : match.answer.has_value();
  BOOL hasProtoAnswer =
      suggestionAnswerMigrationEnabled && isAnswer &&
      GURL(match.answer_template->answers(0).image().url()).is_valid();
  BOOL hasLegacyAnswer = !suggestionAnswerMigrationEnabled && isAnswer &&
                         match.answer->second_line().image_url().is_valid();

  OmniboxIconType iconType = OmniboxIconTypeSuggestionIcon;
  GURL imageURL = GURL();
  if (hasProtoAnswer) {
    imageURL = GURL(match.answer_template->answers(0).image().url());
    iconType = OmniboxIconTypeImage;
  } else if (hasLegacyAnswer) {
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

  OmniboxSuggestionIconType suggestionIconType = IconTypeFromMatch(match);

  return [self initWithIconType:iconType
             suggestionIconType:suggestionIconType
                       isAnswer:isAnswer
                       imageURL:[[CrURL alloc] initWithGURL:imageURL]];
}

@end
