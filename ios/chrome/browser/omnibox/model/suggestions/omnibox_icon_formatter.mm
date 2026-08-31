// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_icon_formatter.h"

#import "base/notreached.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/common/omnibox_feature_configs.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "url/gurl.h"

namespace {

OmniboxSuggestionIconType IconTypeFromMatch(const AutocompleteMatch& match) {
  // Some suggestions have custom icons. Others fallback to the icon from the
  // overall match type.

  if (match.suggest_template && match.suggest_template->has_type_icon()) {
    return GetOmniboxSuggestionIconTypeForSuggestTemplateInfoIconType(
        match.suggest_template->type_icon());
  }

  if (match.IsTrendSuggestion()) {
    return OmniboxSuggestionIconType::kSearchTrend;
  }

  return GetOmniboxSuggestionIconTypeForAutocompleteMatchType(match.type);
}

}  // namespace

@implementation OmniboxIconFormatter

- (instancetype)initWithMatch:(const AutocompleteMatch&)match {
  OmniboxIconType iconType = OmniboxIconTypeSuggestionIcon;
  GURL imageURL = GURL();
  if (match.suggest_template && match.suggest_template->has_type_icon() &&
      match.suggest_template->type_icon() !=
          omnibox::SuggestTemplateInfo_IconType_ICON_TYPE_UNSPECIFIED) {
    iconType = OmniboxIconTypeSuggestionIcon;
    imageURL = GURL();
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
                       isAnswer:NO
                       imageURL:[[CrURL alloc] initWithGURL:imageURL]];
}

@end
