// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/simple_omnibox_icon.h"

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SimpleOmniboxIcon ()

@property(nonatomic, assign) OmniboxIconType iconType;
@property(nonatomic, assign) OmniboxSuggestionIconType suggestionIconType;
@property(nonatomic, assign) BOOL isAnswer;
@property(nonatomic, assign) GURL imageURL;

@end

@implementation SimpleOmniboxIcon

- (instancetype)initWithIconType:(OmniboxIconType)iconType
              suggestionIconType:(OmniboxSuggestionIconType)suggestionIconType
                        isAnswer:(BOOL)isAnswer
                        imageURL:(GURL)imageURL {
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
             suggestionIconType:DEFAULT_FAVICON
                       isAnswer:NO
                       imageURL:GURL()];
}

- (UIImage*)iconImage {
  if (self.suggestionIconType == FALLBACK_ANSWER &&
      self.defaultSearchEngineIsGoogle && [self fallbackAnswerBrandedIcon]) {
    return [[self fallbackAnswerBrandedIcon]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  return GetOmniboxSuggestionIcon(self.suggestionIconType, true);
}

- (BOOL)hasCustomAnswerIcon {
  switch (self.suggestionIconType) {
    case BOOKMARK:
    case DEFAULT_FAVICON:
    case HISTORY:
    case SEARCH:
    case SEARCH_HISTORY:
      return NO;
    case CALCULATOR:
    case CONVERSION:
    case DICTIONARY:
    case STOCK:
    case SUNRISE:
    case LOCAL_TIME:
    case WHEN_IS:
    case TRANSLATION:
      return YES;
    // For the fallback answer, this depends on whether the branded icon exists
    // and whether the default search engine is Google (the icon only exists for
    // Google branding).
    // The default fallback answer icon uses the grey background styling, like
    // the non-answer icons.
    case FALLBACK_ANSWER:
      return self.defaultSearchEngineIsGoogle &&
             [self fallbackAnswerBrandedIcon];
    case OMNIBOX_SUGGESTION_ICON_TYPE_COUNT:
      NOTREACHED();
      return NO;
  }
}

- (UIImage*)fallbackAnswerBrandedIcon {
  return ios::GetChromeBrowserProvider()
      ->GetBrandedImageProvider()
      ->GetOmniboxAnswerIcon();
}

- (UIColor*)iconImageTintColor {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return color::DarkModeDynamicColor(
            [UIColor colorNamed:@"omnibox_suggestion_answer_icon_color"],
            self.incognito,
            [UIColor colorNamed:@"omnibox_suggestion_answer_icon_dark_color"]);
      }
      return color::DarkModeDynamicColor(
          [UIColor colorNamed:@"omnibox_suggestion_icon_color"], self.incognito,
          [UIColor colorNamed:@"omnibox_suggestion_icon_dark_color"]);
    case OmniboxIconTypeFavicon:
      return color::DarkModeDynamicColor(
          [UIColor colorNamed:@"omnibox_suggestion_icon_color"], self.incognito,
          [UIColor colorNamed:@"omnibox_suggestion_icon_dark_color"]);
  }
}

- (UIImage*)backgroundImage {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
      return nil;
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return [[UIImage imageNamed:@"background_solid"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      }
      return nil;
    case OmniboxIconTypeFavicon:
      return nil;
  }
}

- (UIColor*)backgroundImageTintColor {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
      return nil;
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return color::DarkModeDynamicColor([UIColor colorNamed:kBlueColor],
                                           self.incognito,
                                           [UIColor colorNamed:kBlueDarkColor]);
      }
      return nil;
    case OmniboxIconTypeFavicon:
      return nil;
  }
}

- (UIImage*)overlayImage {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
    case OmniboxIconTypeSuggestionIcon:
    case OmniboxIconTypeFavicon:
      return nil;
  }
}

- (UIColor*)overlayImageTintColor {
  return nil;
}

@end
