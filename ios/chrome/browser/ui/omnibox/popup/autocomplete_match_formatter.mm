// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"

#import <UIKit/UIKit.h>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_formatter.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The color of the main text of a suggest cell.
UIColor* SuggestionTextColor(bool incognito) {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextPrimaryColor], incognito,
      [UIColor colorNamed:kTextPrimaryDarkColor]);
}
// The color of the detail text of a suggest cell.
UIColor* SuggestionDetailTextColor(bool incognito) {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextSecondaryColor], incognito,
      [UIColor colorNamed:kTextSecondaryDarkColor]);
}
// The color of the text in the portion of a search suggestion that matches the
// omnibox input text.
UIColor* DimColor() {
  return [UIColor colorWithWhite:(161 / 255.0) alpha:1.0];
}
UIColor* DimColorIncognito() {
  return UIColor.whiteColor;
}

// Temporary convenience accessor for this flag.
// Cleanup along with feature: crbug.com/945334.
bool ShouldUseNewFormatting() {
  return base::FeatureList::IsEnabled(kNewOmniboxPopupLayout);
}
}  // namespace

@implementation AutocompleteMatchFormatter {
  AutocompleteMatch _match;
}

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
  BOOL hasAnswerImage =
      self.hasAnswer && _match.answer->second_line().image_url().is_valid();
  BOOL hasRichEntityImage =
      ShouldUseNewFormatting() && !_match.image_url.empty();
  return hasAnswerImage || hasRichEntityImage;
}

- (BOOL)isURL {
  return !AutocompleteMatch::IsSearchType(_match.type);
}

- (NSAttributedString*)detailText {
  if (self.hasAnswer) {
    if (ShouldUseNewFormatting() &&
        !_match.answer->IsExceptedFromLineReversal()) {
      NSAttributedString* detailBaseText = [self
          attributedStringWithString:base::SysUTF16ToNSString(_match.contents)
                     classifications:&_match.contents_class
                           smallFont:NO
                               color:SuggestionDetailTextColor(self.incognito)
                            dimColor:DimColor()];
      return [self addExtraTextFromAnswerLine:_match.answer->first_line()
                           toAttributedString:detailBaseText
                       useDeemphasizedStyling:YES];
    } else {
      return [self attributedStringWithAnswerLine:_match.answer->second_line()
                           useDeemphasizedStyling:YES];
    }
  } else {
    // The detail text should be the URL (|_match.contents|) for non-search
    // suggestions and the entity type (|_match.description|) for search entity
    // suggestions. For all other search suggestions, |_match.description| is
    // the name of the currently selected search engine, which for mobile we
    // suppress.
    NSString* detailText = nil;
    if (self.isURL)
      detailText = base::SysUTF16ToNSString(_match.contents);
    else if (_match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY)
      detailText = base::SysUTF16ToNSString(_match.description);
    const ACMatchClassifications* classifications =
        self.isURL ? &_match.contents_class : nullptr;
    // The suggestion detail color should match the main text color for entity
    // suggestions. For non-search suggestions (URLs), a highlight color is used
    // instead.
    UIColor* suggestionDetailTextColor = nil;
    if (_match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
      suggestionDetailTextColor = SuggestionTextColor(self.incognito);
    } else {
      suggestionDetailTextColor = SuggestionDetailTextColor(self.incognito);
    }
    DCHECK(suggestionDetailTextColor);
    return [self attributedStringWithString:detailText
                            classifications:classifications
                                  smallFont:YES
                                      color:suggestionDetailTextColor
                                   dimColor:DimColor()];
  }
}

- (id<OmniboxIcon>)icon {
  OmniboxIconFormatter* icon =
      [[OmniboxIconFormatter alloc] initWithMatch:_match];
  icon.defaultSearchEngineIsGoogle = self.defaultSearchEngineIsGoogle;
  icon.incognito = self.incognito;
  return icon;
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
  if (self.hasAnswer) {
    if (ShouldUseNewFormatting() &&
        !_match.answer->IsExceptedFromLineReversal()) {
      return [self attributedStringWithAnswerLine:_match.answer->second_line()
                           useDeemphasizedStyling:NO];
    } else {
      UIColor* suggestionTextColor = SuggestionTextColor(self.incognito);
      UIColor* dimColor = self.incognito ? DimColorIncognito() : DimColor();
      NSAttributedString* attributedBaseText = [self
          attributedStringWithString:base::SysUTF16ToNSString(_match.contents)
                     classifications:&_match.contents_class
                           smallFont:NO
                               color:suggestionTextColor
                            dimColor:dimColor];
      return [self addExtraTextFromAnswerLine:_match.answer->first_line()
                           toAttributedString:attributedBaseText
                       useDeemphasizedStyling:NO];
    }
  } else {
    // The text should be search term (|_match.contents|) for searches,
    // otherwise page title (|_match.description|).
    base::string16 textString =
        !self.isURL ? _match.contents : _match.description;
    NSString* text = base::SysUTF16ToNSString(textString);

    // If for some reason the title is empty, copy the detailText.
    if ([text length] == 0 && [self.detailText length] != 0) {
      text = [self.detailText string];
    }

    const ACMatchClassifications* textClassifications =
        !self.isURL ? &_match.contents_class : &_match.description_class;
    UIColor* suggestionTextColor = SuggestionTextColor(self.incognito);
    UIColor* dimColor = self.incognito ? DimColorIncognito() : DimColor();

    return [self attributedStringWithString:text
                            classifications:textClassifications
                                  smallFont:NO
                                      color:suggestionTextColor
                                   dimColor:dimColor];
  }
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
  if (self.hasAnswer && _match.answer->second_line().image_url().is_valid()) {
    return _match.answer->second_line().image_url();
  } else {
    return GURL(_match.image_url);
  }
}

- (GURL)faviconPageURL {
  return _match.destination_url;
}

- (UIImage*)suggestionTypeIcon {
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

// Create a string to display for an entire answer line.
- (NSAttributedString*)
    attributedStringWithAnswerLine:(const SuggestionAnswer::ImageLine&)line
            useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  NSMutableAttributedString* result =
      [[NSMutableAttributedString alloc] initWithString:@""];

  for (const auto field : line.text_fields()) {
    [result appendAttributedString:
                [self attributedStringForTextfield:&field
                            useDeemphasizedStyling:useDeemphasizedStyling]];
  }

  return [self addExtraTextFromAnswerLine:line
                       toAttributedString:result
                   useDeemphasizedStyling:useDeemphasizedStyling];
}

// Adds the |additional_text| and |status_text| from |line| to the given
// attributed string. This is necessary because answers get their main text
// from the match contents instead of the ImageLine's text_fields. This is
// because those fields contain server-provided formatting, which aren't used.
- (NSAttributedString*)
    addExtraTextFromAnswerLine:(const SuggestionAnswer::ImageLine&)line
            toAttributedString:(NSAttributedString*)attributedString
        useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  NSMutableAttributedString* result = [attributedString mutableCopy];

  NSAttributedString* spacer =
      [[NSAttributedString alloc] initWithString:@"  "];
  if (line.additional_text() != nil) {
    [result appendAttributedString:spacer];
    NSAttributedString* extra =
        [self attributedStringForTextfield:line.additional_text()
                    useDeemphasizedStyling:useDeemphasizedStyling];
    [result appendAttributedString:extra];
  }

  if (line.status_text() != nil) {
    [result appendAttributedString:spacer];
    [result appendAttributedString:
                [self attributedStringForTextfield:line.status_text()
                            useDeemphasizedStyling:useDeemphasizedStyling]];
  }

  return result;
}

// Create a string to display for a textual part ("textfield") of a suggestion
// answer.
- (NSAttributedString*)
    attributedStringForTextfield:(const SuggestionAnswer::TextField*)field
          useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  const base::string16& string = field->text();

  NSString* unescapedString =
      base::SysUTF16ToNSString(net::UnescapeForHTML(string));

  NSDictionary* attributes =
      ShouldUseNewFormatting()
          ? [self formattingAttributesForSuggestionStyle:field->style()
                                  useDeemphasizedStyling:useDeemphasizedStyling]
          : [self attributesForSuggestionType:field->type()];

  return [[NSAttributedString alloc] initWithString:unescapedString
                                         attributes:attributes];
}

- (NSDictionary<NSAttributedStringKey, id>*)attributesForSuggestionType:
    (int)type {
  DCHECK(!ShouldUseNewFormatting());
  // Answer types, sizes and colors specified at http://goto.google.com/ais_api.
  UIColor* detailTextColor = SuggestionDetailTextColor(self.incognito);
  switch (type) {
    case SuggestionAnswer::TOP_ALIGNED:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:12],
        NSBaselineOffsetAttributeName : @10.0f,
        NSForegroundColorAttributeName : detailTextColor,
      };
    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:16],
        NSForegroundColorAttributeName : [UIColor colorNamed:kGreenColor],
      };
    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:16],
        NSForegroundColorAttributeName : [UIColor colorNamed:kRedColor],
      };
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:16],
      };
    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:20],

        NSForegroundColorAttributeName : detailTextColor,
      };
    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:24],
        NSForegroundColorAttributeName : detailTextColor,
      };
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:12],
        NSForegroundColorAttributeName : detailTextColor,
      };
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:14],
        NSForegroundColorAttributeName : detailTextColor,
      };
    case SuggestionAnswer::SUGGESTION:
      // Fall through.
    default:
      return @{
        NSFontAttributeName : [UIFont systemFontOfSize:16],
        NSForegroundColorAttributeName : SuggestionTextColor(self.incognito),
      };
  }
}

// Return correct formatting attributes for the given style.
// |useDeemphasizedStyling| is necessary because some styles (e.g. SUPERIOR)
// should take their color from the surrounding line; they don't have a fixed
// color.
- (NSDictionary<NSAttributedStringKey, id>*)
    formattingAttributesForSuggestionStyle:(SuggestionAnswer::TextStyle)style
                    useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  DCHECK(ShouldUseNewFormatting());
  UIFontDescriptor* defaultFontDescriptor =
      useDeemphasizedStyling
          ? [[UIFontDescriptor
                preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline]
                fontDescriptorWithSymbolicTraits:
                    UIFontDescriptorTraitTightLeading]
          : [UIFontDescriptor
                preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  UIColor* defaultColor = useDeemphasizedStyling
                              ? SuggestionDetailTextColor(self.incognito)
                              : SuggestionTextColor(self.incognito);

  switch (style) {
    case SuggestionAnswer::TextStyle::NORMAL:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : defaultColor,
      };
    case SuggestionAnswer::TextStyle::NORMAL_DIM:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : UIColor.grayColor,
      };
    case SuggestionAnswer::TextStyle::SECONDARY:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : UIColor.grayColor,
      };
    case SuggestionAnswer::TextStyle::BOLD: {
      UIFontDescriptor* boldFontDescriptor = [defaultFontDescriptor
          fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:boldFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : defaultColor,
      };
    }
    case SuggestionAnswer::TextStyle::POSITIVE:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : [UIColor colorNamed:kGreenColor],
      };
    case SuggestionAnswer::TextStyle::NEGATIVE:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : [UIColor colorNamed:kRedColor],
      };
    case SuggestionAnswer::TextStyle::SUPERIOR: {
      // Calculate a slightly smaller font. The ratio here is somewhat
      // arbitrary. Proportions from 5/9 to 5/7 all look pretty good.
      CGFloat ratio = 5.0 / 9.0;
      UIFont* defaultFont = [UIFont fontWithDescriptor:defaultFontDescriptor
                                                  size:0];
      UIFontDescriptor* superiorFontDescriptor = [defaultFontDescriptor
          fontDescriptorWithSize:defaultFontDescriptor.pointSize * ratio];
      CGFloat baselineOffset =
          defaultFont.capHeight - defaultFont.capHeight * ratio;
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:superiorFontDescriptor
                                                    size:0],
        NSBaselineOffsetAttributeName :
            [NSNumber numberWithFloat:baselineOffset],
        NSForegroundColorAttributeName : defaultColor,
      };
    }
    case SuggestionAnswer::TextStyle::NONE:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : defaultColor,
      };
  }
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

  UIFont* fontRef;
  if (ShouldUseNewFormatting()) {
    fontRef =
        smallFont
            ? [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
            : [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  } else {
    fontRef =
        smallFont ? [UIFont systemFontOfSize:15] : [UIFont systemFontOfSize:17];
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
    UIFont* boldFontRef;
    if (ShouldUseNewFormatting()) {
      UIFontDescriptor* fontDescriptor = fontRef.fontDescriptor;
      UIFontDescriptor* boldFontDescriptor = [fontDescriptor
          fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
      boldFontRef = [UIFont fontWithDescriptor:boldFontDescriptor size:0];
    } else {
      UIFontWeight boldFontWeight = UIFontWeightMedium;
      boldFontRef = [UIFont systemFontOfSize:fontRef.pointSize
                                      weight:boldFontWeight];
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
