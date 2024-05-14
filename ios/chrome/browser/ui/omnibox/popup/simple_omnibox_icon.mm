// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/simple_omnibox_icon.h"

#import "base/notreached.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "url/gurl.h"

@interface SimpleOmniboxIcon ()

@property(nonatomic, assign) OmniboxIconType iconType;
@property(nonatomic, assign) OmniboxSuggestionIconType suggestionIconType;
@property(nonatomic, assign) BOOL isAnswer;
@property(nonatomic, strong) CrURL* imageURL;

@end

@implementation SimpleOmniboxIcon

- (instancetype)initWithIconType:(OmniboxIconType)iconType
              suggestionIconType:(OmniboxSuggestionIconType)suggestionIconType
                        isAnswer:(BOOL)isAnswer
                        imageURL:(CrURL*)imageURL {
  self = [super init];
  if (self) {
    _iconType = iconType;
    _suggestionIconType = suggestionIconType;
    _isAnswer = isAnswer;
    _imageURL = imageURL;
  }
  return self;
}

- (instancetype)init {
  return [self initWithIconType:OmniboxIconTypeSuggestionIcon
             suggestionIconType:OmniboxSuggestionIconType::kDefaultFavicon
                       isAnswer:NO
                       imageURL:[[CrURL alloc] initWithGURL:GURL()]];
}

- (UIImage*)iconImage {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  if (self.suggestionIconType == OmniboxSuggestionIconType::kFallbackAnswer &&
      self.defaultSearchEngineIsGoogle) {
    return GetBrandedGoogleIconForOmnibox();
  }
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return GetOmniboxSuggestionIcon(self.suggestionIconType);
}

- (BOOL)hasCustomAnswerIcon {
  switch (self.suggestionIconType) {
    case OmniboxSuggestionIconType::kDefaultFavicon:
    case OmniboxSuggestionIconType::kSearch:
    case OmniboxSuggestionIconType::kSearchHistory:
    case OmniboxSuggestionIconType::kSearchTrend:
      return NO;
    case OmniboxSuggestionIconType::kCalculator:
    case OmniboxSuggestionIconType::kConversion:
    case OmniboxSuggestionIconType::kDictionary:
    case OmniboxSuggestionIconType::kStock:
    case OmniboxSuggestionIconType::kSunrise:
    case OmniboxSuggestionIconType::kWhenIs:
    case OmniboxSuggestionIconType::kTranslation:
      return YES;
    // For the fallback answer, this depends on whether the branded icon exists
    // and whether the default search engine is Google (the icon only exists for
    // Google branding).
    // The default fallback answer icon uses the grey background styling, like
    // the non-answer icons.
    case OmniboxSuggestionIconType::kFallbackAnswer:
      return self.defaultSearchEngineIsGoogle &&
             [self fallbackAnswerBrandedIcon];
    case OmniboxSuggestionIconType::kCount:
      NOTREACHED_IN_MIGRATION();
      return NO;
  }
}

- (UIImage*)fallbackAnswerBrandedIcon {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kOmniboxAnswer);
}

- (UIColor*)iconImageTintColor {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return [UIColor colorNamed:@"omnibox_suggestion_answer_icon_color"];
      }
      return [UIColor colorNamed:@"omnibox_suggestion_icon_color"];
    case OmniboxIconTypeFavicon:
      return [UIColor colorNamed:@"omnibox_suggestion_icon_color"];
  }
}

- (UIColor*)backgroundImageTintColor {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
      return nil;
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return [UIColor colorNamed:kBlueColor];
      }
      return nil;
    case OmniboxIconTypeFavicon:
      return nil;
  }
}

- (UIColor*)borderColor {
  return nil;
}

@end
