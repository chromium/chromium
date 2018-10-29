// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/autocomplete_match_formatter.h"

#import <UIKit/UIKit.h>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The color of the main text of a suggest cell.
UIColor* SuggestionTextColor() {
  if (IsUIRefreshPhase1Enabled()) {
    return [UIColor blackColor];
  } else {
    return [UIColor colorWithWhite:(51 / 255.0) alpha:1.0];
  }
}
// The color of the detail text of a suggest cell.
UIColor* SuggestionDetailTextColor() {
  if (IsUIRefreshPhase1Enabled()) {
    return [UIColor colorWithWhite:0 alpha:0.41];
  } else {
    return [UIColor colorWithRed:(85 / 255.0)
                           green:(149 / 255.0)
                            blue:(254 / 255.0)
                           alpha:1.0];
  }
}
// The color of the detail text of a suggest cell.
UIColor* SuggestionDetailTextColorIncognito() {
  if (IsUIRefreshPhase1Enabled()) {
    return [UIColor colorWithWhite:1 alpha:0.5];
  } else {
    return [UIColor colorWithRed:(85 / 255.0)
                           green:(149 / 255.0)
                            blue:(254 / 255.0)
                           alpha:1.0];
  }
}
// The color of the text in the portion of a search suggestion that matches the
// omnibox input text.
UIColor* DimColor() {
  return [UIColor colorWithWhite:(161 / 255.0) alpha:1.0];
}
UIColor* SuggestionTextColorIncognito() {
  return [UIColor whiteColor];
}

UIColor* DimColorIncognito() {
  return [UIColor whiteColor];
}
}  // namespace

@implementation AutocompleteMatchFormatter {
  AutocompleteMatch _match;
}
@synthesize incognito = _incognito;
@synthesize starred = _starred;

- (instancetype)initWithMatch:(const AutocompleteMatch&)match {
  self = [super init];
  if (self) {
    _match = AutocompleteMatch(match);
  }
  return self;
}

+ (instancetype)formatterWithMatch:(const AutocompleteMatch&)match {
  return [[self alloc] initWithMatch:match];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"%@ (%@)", self.text.string, self.detailText.string];
}

#pragma mark AutocompleteSuggestion

- (BOOL)supportsDeletion {
  return _match.SupportsDeletion();
}

- (BOOL)hasAnswer {
  return _match.answer.has_value();
}

- (BOOL)hasImage {
  return self.hasAnswer && _match.answer->second_line().image_url().is_valid();
}

- (BOOL)isURL {
  return !AutocompleteMatch::IsSearchType(_match.type);
}

- (NSAttributedString*)detailText {
  // The detail text should be the URL (|_match.contents|) for non-search
  // suggestions and the entity type (|_match.description|) for search entity
  // suggestions. For all other search suggestions, |_match.description| is the
  // name of the currently selected search engine, which for mobile we suppress.
  NSString* detailText = nil;
  if (self.isURL)
    detailText = base::SysUTF16ToNSString(_match.contents);
  else if (_match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY)
    detailText = base::SysUTF16ToNSString(_match.description);

  NSAttributedString* detailAttributedText = nil;
  if (self.hasAnswer) {
    detailAttributedText =
        [self attributedStringWithAnswerLine:_match.answer->second_line()];
  } else {
    const ACMatchClassifications* classifications =
        self.isURL ? &_match.contents_class : nullptr;
    // The suggestion detail color should match the main text color for entity
    // suggestions. For non-search suggestions (URLs), a highlight color is used
    // instead.
    UIColor* suggestionDetailTextColor = nil;
    if (_match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
      suggestionDetailTextColor =
          _incognito ? SuggestionTextColorIncognito() : SuggestionTextColor();
    } else {
      suggestionDetailTextColor = _incognito
                                      ? SuggestionDetailTextColorIncognito()
                                      : SuggestionDetailTextColor();
    }
    DCHECK(suggestionDetailTextColor);
    detailAttributedText =
        [self attributedStringWithString:detailText
                         classifications:classifications
                               smallFont:YES
                                   color:suggestionDetailTextColor
                                dimColor:DimColor()];
  }
  return detailAttributedText;
}

- (NSInteger)numberOfLines {
  // Answers specify their own limit on the number of lines to show but are
  // additionally capped here at 3 to guard against unreasonable values.
  const SuggestionAnswer::TextField& first_text_field =
      _match.answer->second_line().text_fields()[0];
  if (first_text_field.has_num_lines() && first_text_field.num_lines() > 1)
    return MIN(3, first_text_field.num_lines());
  else
    return 1;
}

- (NSAttributedString*)text {
  // The text should be search term (|_match.contents|) for searches, otherwise
  // page title (|_match.description|).
  base::string16 textString =
      !self.isURL ? _match.contents : _match.description;
  NSString* text = base::SysUTF16ToNSString(textString);

  // If for some reason the title is empty, copy the detailText.
  if ([text length] == 0 && [self.detailText length] != 0) {
    text = [self.detailText string];
  }

  NSAttributedString* attributedText = nil;

  if (self.hasAnswer) {
    attributedText =
        [self attributedStringWithAnswerLine:_match.answer->first_line()];
  } else {
    const ACMatchClassifications* textClassifications =
        !self.isURL ? &_match.contents_class : &_match.description_class;
    UIColor* suggestionTextColor =
        _incognito ? SuggestionTextColorIncognito() : SuggestionTextColor();
    UIColor* dimColor = _incognito ? DimColorIncognito() : DimColor();

    attributedText = [self attributedStringWithString:text
                                      classifications:textClassifications
                                            smallFont:NO
                                                color:suggestionTextColor
                                             dimColor:dimColor];
  }
  return attributedText;
}

// The primary purpose of this list is to omit the "what you typed" types, since
// those are simply the input in the omnibox and copying the text back to the
// omnibox would be a noop. However, this list also omits other types that are
// deprecated or not launched on iOS.
- (BOOL)isAppendable {
  return _match.type == AutocompleteMatchType::BOOKMARK_TITLE ||
         _match.type == AutocompleteMatchType::CALCULATOR ||
         _match.type == AutocompleteMatchType::HISTORY_BODY ||
         _match.type == AutocompleteMatchType::HISTORY_KEYWORD ||
         _match.type == AutocompleteMatchType::HISTORY_TITLE ||
         _match.type == AutocompleteMatchType::HISTORY_URL ||
         _match.type == AutocompleteMatchType::NAVSUGGEST ||
         _match.type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL ||
         _match.type == AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED;
}

- (GURL)imageURL {
  return _match.answer->second_line().image_url();
}

- (int)imageID {
  // This method returns old icons. It should only be used in pre-UI Refresh.
  DCHECK(!IsUIRefreshPhase1Enabled());

  return GetIconForAutocompleteMatchType(_match.type, self.isStarred,
                                         self.isIncognito);
}

- (UIImage*)suggestionTypeIcon {
  DCHECK(IsUIRefreshPhase1Enabled());
  DCHECK(
      !(self.isIncognito && _match.type == AutocompleteMatchType::CALCULATOR))
      << "Calculator answers are never shown in incognito mode because input "
         "is never sent to the search provider.";
  return GetOmniboxSuggestionIconForAutocompleteMatchType(_match.type,
                                                          self.isStarred);
}

- (BOOL)isTabMatch {
  return _match.has_tab_match;
}

#pragma mark helpers

// Create a string to display for an answer line.
- (NSMutableAttributedString*)attributedStringWithAnswerLine:
    (const SuggestionAnswer::ImageLine&)line {
  NSMutableAttributedString* result =
      [[NSMutableAttributedString alloc] initWithString:@""];

  for (const auto field : line.text_fields()) {
    [result appendAttributedString:[self attributedStringForTextfield:&field]];
  }

  NSAttributedString* spacer =
      [[NSAttributedString alloc] initWithString:@"  "];
  if (line.additional_text() != nil) {
    [result appendAttributedString:spacer];
    [result appendAttributedString:
                [self attributedStringForTextfield:line.additional_text()]];
  }

  if (line.status_text() != nil) {
    [result appendAttributedString:spacer];
    [result appendAttributedString:
                [self attributedStringForTextfield:line.status_text()]];
  }

  return result;
}

// Create a string to display for a textual part ("textfield") of a suggestion
// answer.
- (NSAttributedString*)attributedStringForTextfield:
    (const SuggestionAnswer::TextField*)field {
  const base::string16& string = field->text();
  const int type = field->type();

  NSDictionary* attributes = nil;

  // Answer types, sizes and colors specified at http://goto.google.com/ais_api.
  switch (type) {
    case SuggestionAnswer::TOP_ALIGNED:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:12]
                : [[MDCTypography fontLoader] regularFontOfSize:12],
        NSBaselineOffsetAttributeName : @10.0f,
        NSForegroundColorAttributeName : [UIColor grayColor],
      };
      break;
    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:16]
                : [[MDCTypography fontLoader] regularFontOfSize:16],
        NSForegroundColorAttributeName : [UIColor colorWithRed:11 / 255.0
                                                         green:128 / 255.0
                                                          blue:67 / 255.0
                                                         alpha:1.0],
      };
      break;
    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:16]
                : [[MDCTypography fontLoader] regularFontOfSize:16],
        NSForegroundColorAttributeName : [UIColor colorWithRed:197 / 255.0
                                                         green:57 / 255.0
                                                          blue:41 / 255.0
                                                         alpha:1.0],
      };
      break;
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:16]
                : [[MDCTypography fontLoader] regularFontOfSize:16],
      };
      break;
    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:20]
                : [[MDCTypography fontLoader] regularFontOfSize:20],
        NSForegroundColorAttributeName : [UIColor grayColor],
      };
      break;
    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:24]
                : [[MDCTypography fontLoader] regularFontOfSize:24],
        NSForegroundColorAttributeName : [UIColor grayColor],
      };
      break;
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:12]
                : [[MDCTypography fontLoader] regularFontOfSize:12],
        NSForegroundColorAttributeName : [UIColor grayColor],
      };
      break;
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:14]
                : [[MDCTypography fontLoader] regularFontOfSize:14],
        NSForegroundColorAttributeName : [UIColor grayColor],
      };
      break;
    case SuggestionAnswer::SUGGESTION:
    // Fall through.
    default:
      attributes = @{
        NSFontAttributeName :
            IsUIRefreshPhase1Enabled()
                ? [UIFont systemFontOfSize:16]
                : [[MDCTypography fontLoader] regularFontOfSize:16],
      };
  }

  NSString* unescapedString =
      base::SysUTF16ToNSString(net::UnescapeForHTML(string));
  // TODO(crbug.com/763894): Remove this tag stripping once the JSON parsing
  // class handles HTML tags.
  unescapedString = [unescapedString stringByReplacingOccurrencesOfString:@"<b>"
                                                               withString:@""];
  unescapedString =
      [unescapedString stringByReplacingOccurrencesOfString:@"</b>"
                                                 withString:@""];

  return [[NSAttributedString alloc] initWithString:unescapedString
                                         attributes:attributes];
}

// Create a formatted string given text and classifications.
- (NSMutableAttributedString*)
attributedStringWithString:(NSString*)text
           classifications:(const ACMatchClassifications*)classifications
                 smallFont:(BOOL)smallFont
                     color:(UIColor*)defaultColor
                  dimColor:(UIColor*)dimColor {
  if (text == nil)
    return nil;

  UIFont* fontRef =
      smallFont ? [MDCTypography body1Font] : [MDCTypography subheadFont];
  if (IsUIRefreshPhase1Enabled()) {
    fontRef =
        smallFont ? [UIFont systemFontOfSize:12] : [UIFont systemFontOfSize:17];
  }

  NSMutableAttributedString* styledText =
      [[NSMutableAttributedString alloc] initWithString:text];

  // Set the base attributes to the default font and color.
  NSDictionary* dict = @{
    NSFontAttributeName : fontRef,
    NSForegroundColorAttributeName : defaultColor,
  };
  [styledText addAttributes:dict range:NSMakeRange(0, [text length])];

  if (classifications != NULL) {
    UIFont* boldFontRef =
        [[MDCTypography fontLoader] mediumFontOfSize:fontRef.pointSize];
    if (IsUIRefreshPhase1Enabled()) {
      boldFontRef =
          [UIFont systemFontOfSize:fontRef.pointSize weight:UIFontWeightMedium];
    }

    for (ACMatchClassifications::const_iterator i = classifications->begin();
         i != classifications->end(); ++i) {
      const BOOL isLast = (i + 1) == classifications->end();
      const size_t nextOffset = (isLast ? [text length] : (i + 1)->offset);
      const NSInteger location = static_cast<NSInteger>(i->offset);
      const NSInteger length = static_cast<NSInteger>(nextOffset - i->offset);
      // Guard against bad, off-the-end classification ranges due to
      // crbug.com/121703 and crbug.com/131370.
      if (i->offset + length > [text length] || length <= 0)
        break;
      const NSRange range = NSMakeRange(location, length);
      if (0 != (i->style & ACMatchClassification::MATCH)) {
        [styledText addAttribute:NSFontAttributeName
                           value:boldFontRef
                           range:range];
      }

      if (0 != (i->style & ACMatchClassification::DIM)) {
        [styledText addAttribute:NSForegroundColorAttributeName
                           value:dimColor
                           range:range];
      }
    }
  }
  return styledText;
}

@end
