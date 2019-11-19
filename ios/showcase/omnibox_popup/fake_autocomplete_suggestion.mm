// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/fake_autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/simple_omnibox_icon.h"

#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacer attributed string for dividing parts of an autocomplete suggestion's
// text and detail text.
NSAttributedString* spacer() {
  return [[NSAttributedString alloc] initWithString:@"  "];
}

// Standard attributed string for the text part of a suggestion.
NSAttributedString* textString(NSString* text) {
  return [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:17],
            NSForegroundColorAttributeName : [UIColor blackColor],
          }];
}

// Standard attributed string for the detail part of a suggestion.
NSAttributedString* detailTextString(NSString* detailText) {
  return [[NSAttributedString alloc]
      initWithString:detailText
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSForegroundColorAttributeName : [UIColor colorWithWhite:0
                                                               alpha:0.41],
          }];
}

// Main text for an autocomplete suggestion representing weather
NSAttributedString* weatherText() {
  return [[NSAttributedString alloc]
      initWithString:@"weather"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

// Detail text for an autocomplete suggestion representing weather
NSAttributedString* weatherDetailText() {
  NSAttributedString* number = [[NSAttributedString alloc]
      initWithString:@"18"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:24],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSAttributedString* degreeSymbol = [[NSAttributedString alloc]
      initWithString:@"°C"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSBaselineOffsetAttributeName : @10.0f,
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSAttributedString* date = [[NSAttributedString alloc]
      initWithString:@"ven."
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];

  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:number];
  [answer appendAttributedString:degreeSymbol];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:date];

  return [answer copy];
}

// Main text for an autocomplete suggestion representing stock price
NSAttributedString* stockText() {
  NSAttributedString* search = [[NSAttributedString alloc]
      initWithString:@"goog stock"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
  NSAttributedString* priceSource = [[NSAttributedString alloc]
      initWithString:@"GOOG (NASDAQ), 13:18 UTC−4"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:12],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:search];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:priceSource];
  return [answer copy];
}

// Detail text for an autocomplete suggestion representing stock price
NSAttributedString* stockDetailText() {
  NSAttributedString* price = [[NSAttributedString alloc]
      initWithString:@"1 209,29"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:24],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSAttributedString* priceChange = [[NSAttributedString alloc]
      initWithString:@"-22,25 (-1,81%)"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
            NSForegroundColorAttributeName : [UIColor colorWithRed:197 / 255.0
                                                             green:57 / 255.0
                                                              blue:41 / 255.0
                                                             alpha:1.0],
          }];
  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:price];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:priceChange];
  return [answer copy];
}

// Main text for an autocomplete suggestion representing a word definition
NSAttributedString* definitionText() {
  NSAttributedString* searchText = [[NSAttributedString alloc]
      initWithString:@"define government"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
  NSAttributedString* pronunciation = [[NSAttributedString alloc]
      initWithString:@"• /ˈɡʌv(ə)nˌm(ə)nt/"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:14],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
  NSMutableAttributedString* answer =
      [[NSMutableAttributedString alloc] initWithAttributedString:searchText];
  [answer appendAttributedString:spacer()];
  [answer appendAttributedString:pronunciation];
  return [answer copy];
}

// Detail text for an autocomplete suggestion representing a word definition
NSAttributedString* definitionDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"the group of people with the authority to govern a "
                     @"country or state; a particular ministry in office. "
                     @"Let's expand this definition to get to three lines also."
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:14],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* sunriseText() {
  return [[NSAttributedString alloc]
      initWithString:@"sunrise in paris"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

NSAttributedString* sunriseDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"06:35"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:24],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* knowledgeText() {
  return [[NSAttributedString alloc]
      initWithString:@"how high is mount everest"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

NSAttributedString* knowledgeDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"8 848 m"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:24],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* sportsText() {
  return [[NSAttributedString alloc]
      initWithString:@"boston celtics"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

NSAttributedString* sportsDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@" contre Pacers 30 mars à 00:00 UTC+1"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:14],
            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* whenIsText() {
  return [[NSAttributedString alloc]
      initWithString:@"when is bastille day"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

NSAttributedString* whenIsDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"dimanche 14 juillet 2019"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:20],

            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* currencyText() {
  return [[NSAttributedString alloc]
      initWithString:@"100 usd"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

NSAttributedString* currencyDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"100 Dollar américain = 89.01 Euro"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:20],

            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* translateText() {
  return [[NSAttributedString alloc]
      initWithString:@"bonjour in chinese"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:16],
          }];
}

NSAttributedString* translateDetailText() {
  return [[NSAttributedString alloc]
      initWithString:@"你好 (Chinois (simplifié))"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:20],

            NSForegroundColorAttributeName : [UIColor grayColor],
          }];
}

NSAttributedString* calculatorText() {
  return [[NSAttributedString alloc]
      initWithString:@"= 3.46410162"
          attributes:@{
            NSFontAttributeName : [UIFont systemFontOfSize:17
                                                    weight:UIFontWeightMedium],
            NSForegroundColorAttributeName : [UIColor blackColor],
          }];
}
}  // namespace

@implementation FakeAutocompleteSuggestion

- (instancetype)init {
  self = [super init];
  if (self) {
    _isURL = YES;
    _text = [[NSAttributedString alloc] initWithString:@""];
    _detailText = [[NSAttributedString alloc] initWithString:@""];
    _numberOfLines = 1;
    _suggestionTypeIcon =
        [[UIImage imageNamed:@"omnibox_completion_default_favicon"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _icon = [[SimpleOmniboxIcon alloc] init];
  }
  return self;
}

// In the new popup, this field is not used. Instead, the icon field is used.
- (GURL)imageURL {
  return GURL();
}

// In the new popup, this field is not used. Instead, the icon field is used.
- (GURL)faviconPageURL {
  return GURL();
}

// In the new popup, this field is not used. Instead, the icon field, which
// always has an image, is used.
- (BOOL)hasImage {
  return self.imageURL.is_valid();
}

+ (instancetype)simpleSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = textString(@"Simple suggestion");
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:SEARCH
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)suggestionWithDetail {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = textString(@"Suggestion with detail");
  suggestion.detailText = detailTextString(@"Detail");
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:SEARCH
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)clippingSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text =
      textString(@"Suggestion with text that clips because it is very long "
                 @"and extends off the right end of the screen");
  suggestion.detailText = detailTextString(
      @"Detail about the suggestion that also clips because it is too long "
      @"for the screen and extends off of the right edge.");
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:SEARCH
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)appendableSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = textString(@"Appendable suggestion");
  suggestion.isAppendable = true;
  return suggestion;
}

+ (instancetype)otherTabSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = textString(@"Other tab suggestion");
  suggestion.isTabMatch = true;
  return suggestion;
}

+ (instancetype)deletableSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = textString(@"Deletable suggestion");
  suggestion.supportsDeletion = YES;
  return suggestion;
}

+ (instancetype)weatherSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = weatherText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = weatherDetailText();
  // The image currently doesn't display because there is no fake
  // Image Retriever, but leaving this here in case this is ever necessary.
  suggestion.icon = [[SimpleOmniboxIcon alloc]
        initWithIconType:OmniboxIconTypeImage
      suggestionIconType:DEFAULT_FAVICON
                isAnswer:NO
                imageURL:GURL("https://ssl.gstatic.com/onebox/weather/128/"
                              "sunny.png")];
  return suggestion;
}

+ (instancetype)stockSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = stockText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = stockDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:STOCK
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)definitionSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = definitionText();
  suggestion.numberOfLines = 3;
  suggestion.hasAnswer = YES;
  suggestion.detailText = definitionDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:DICTIONARY
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)sunriseSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = sunriseText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = sunriseDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:SUNRISE
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)knowledgeSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = knowledgeText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = knowledgeDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:FALLBACK_ANSWER
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)sportsSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = sportsText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = sportsDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:FALLBACK_ANSWER
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)whenIsSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = whenIsText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = whenIsDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:WHEN_IS
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)currencySuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = currencyText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = currencyDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:CONVERSION
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)translateSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = translateText();
  suggestion.hasAnswer = YES;
  suggestion.detailText = translateDetailText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:TRANSLATION
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)calculatorSuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = calculatorText();
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeSuggestionIcon
                               suggestionIconType:CALCULATOR
                                         isAnswer:NO
                                         imageURL:GURL()];
  return suggestion;
}

+ (instancetype)richEntitySuggestion {
  FakeAutocompleteSuggestion* suggestion =
      [[FakeAutocompleteSuggestion alloc] init];
  suggestion.text = textString(@"Avengers : Endgame");
  suggestion.detailText = detailTextString(@"Film (2019)");
  // The image currently doesn't display because there is no fake
  // Image Retriever, but leaving this here in case this is ever necessary.
  GURL imageURL = GURL("https://encrypted-tbn0.gstatic.com/"
                       "images?q=tbn:ANd9GcRl35jshKCRWt76yUSKh5r0_"
                       "BRbWuSU1uZOCGnzq95nJ8yXUg913LciCgz-s3reyfACsrAAYg");
  suggestion.icon =
      [[SimpleOmniboxIcon alloc] initWithIconType:OmniboxIconTypeImage
                               suggestionIconType:SEARCH
                                         isAnswer:NO
                                         imageURL:imageURL];
  return suggestion;
}

@end
