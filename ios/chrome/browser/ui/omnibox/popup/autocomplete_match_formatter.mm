// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"

#import <UIKit/UIKit.h>

#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "components/omnibox/browser/omnibox_feature_configs.h"
#import "components/omnibox/browser/suggestion_answer.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

/// The color of the main text of a suggest cell.
UIColor* SuggestionTextColor() {
  return [UIColor colorNamed:kTextPrimaryColor];
}
/// The color of the detail text of a suggest cell.
UIColor* SuggestionDetailTextColor() {
  return [UIColor colorNamed:kTextSecondaryColor];
}
/// The color of the text in the portion of a search suggestion that matches the
/// omnibox input text.
UIColor* DimColor() {
  return [UIColor colorWithWhite:(161 / 255.0) alpha:1.0];
}
UIColor* DimColorIncognito() {
  return UIColor.whiteColor;
}

}  // namespace

@implementation AutocompleteMatchFormatter {
  AutocompleteMatch _match;
}
@synthesize suggestionSectionId;
@synthesize actionsInSuggest;

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

+ (NSAttributedString*)spacerAttributedString {
  return [[NSAttributedString alloc] initWithString:@"  "];
}

#pragma mark - NSObject

- (NSString*)description {
  NSString* description = [NSString
      stringWithFormat:@"<%@ %p> %@ (%@)", self.class, self,
                       self.text.string, self.detailText.string];
  if (self.pedalData) {
    description =
        [description stringByAppendingFormat:@" P:[%@]", self.pedalData.title];
  }
  return description;
}

#pragma mark AutocompleteSuggestion

- (BOOL)supportsDeletion {
  return _match.SupportsDeletion();
}

- (BOOL)hasAnswer {
  if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled) {
    return _match.answer_template.has_value();
  }
  return _match.answer.has_value();
}

- (BOOL)isURL {
  return !AutocompleteMatch::IsSearchType(_match.type);
}

- (NSAttributedString*)detailText {
  if (self.hasAnswer) {
    return [self answerDetailText];
  } else {
    // The detail text should be the URL (`_match.contents`) for non-search
    // suggestions and the entity type (`_match.description`) for search entity
    // suggestions. For all other search suggestions, `_match.description` is
    // the name of the currently selected search engine, which for mobile we
    // suppress.
    NSString* detailText = nil;
    if (self.isURL) {
      detailText = base::SysUTF16ToNSString(_match.contents);
    } else if (_match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
      detailText = base::SysUTF16ToNSString(_match.description);
    }

    if (!detailText.length) {
      return nil;
    }
    const ACMatchClassifications* classifications =
        self.isURL ? &_match.contents_class : nullptr;
    // The suggestion detail color should match the main text color for entity
    // suggestions. For non-search suggestions (URLs), a highlight color is used
    // instead.
    UIColor* suggestionDetailTextColor = nil;
    if (_match.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
      suggestionDetailTextColor = SuggestionDetailTextColor();
    } else {
      suggestionDetailTextColor = SuggestionTextColor();
    }
    DCHECK(suggestionDetailTextColor);
    return [self attributedStringWithString:detailText
                            classifications:classifications
                                  smallFont:YES
                                      color:suggestionDetailTextColor
                                   dimColor:DimColor()];
  }
}

- (NSAttributedString*)answerDetailText {
  DCHECK(self.hasAnswer);
  if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled) {
    NSMutableAttributedString* result =
        [[NSMutableAttributedString alloc] initWithString:@""];
    NSAttributedString* spacer = [[self class] spacerAttributedString];

    if (_match.answer_type == omnibox::ANSWER_TYPE_DICTIONARY) {
      auto subheadFragments =
          _match.answer_template->answers(0).subhead().fragments();

      for (auto fragment : subheadFragments) {
        NSAttributedString* fragmentText =
            [self attributedStringForFragment:fragment
                                        color:SuggestionDetailTextColor()
                       useDeemphasizedStyling:YES];
        [result appendAttributedString:fragmentText];
        [result appendAttributedString:spacer];
      }
    } else {
      auto headLineFragments =
          _match.answer_template->answers(0).headline().fragments();

      for (NSInteger i = 0; i < headLineFragments.size(); i++) {
        NSAttributedString* fragmentText;
        // The first fragment has a html bold tag so we skip it and use the
        // match contents instead. The match contents has the first fragment
        // text without the bold tag (eg. match contents : abc , first fragment
        // : ab<b>c</b>).
        // TODO(crbug.com/343706167): Follow up on removing the bold tag from
        // the first fragment.
        if (i == 0) {
          fragmentText =
              [self attributedStringWithString:base::SysUTF16ToNSString(
                                                   _match.contents)
                               classifications:&_match.contents_class
                                     smallFont:NO
                                         color:SuggestionDetailTextColor()
                                      dimColor:DimColor()];
        } else {
          fragmentText =
              [self attributedStringForFragment:headLineFragments[i]
                                          color:SuggestionDetailTextColor()
                         useDeemphasizedStyling:YES];
        }

        [result appendAttributedString:fragmentText];
        [result appendAttributedString:spacer];
      }
    }

    // Remove the extra spacer.
    if (result.length > 0) {
      NSRange lastCharacterRange =
          NSMakeRange(result.length - spacer.length, spacer.length);
      [result deleteCharactersInRange:lastCharacterRange];
    }

    return result;
  } else {
    if (!_match.answer->IsExceptedFromLineReversal(_match.answer_type)) {
      NSAttributedString* detailBaseText = [self
          attributedStringWithString:base::SysUTF16ToNSString(_match.contents)
                     classifications:&_match.contents_class
                           smallFont:NO
                               color:SuggestionDetailTextColor()
                            dimColor:DimColor()];
      return [self addExtraTextFromAnswerLine:_match.answer->first_line()
                           toAttributedString:detailBaseText
                       useDeemphasizedStyling:YES];
    }
    return [self attributedStringWithAnswerLine:_match.answer->second_line()
                         useDeemphasizedStyling:YES];
  }
}

- (id<OmniboxIcon>)icon {
  OmniboxIconFormatter* icon =
      [[OmniboxIconFormatter alloc] initWithMatch:_match];
  icon.defaultSearchEngineIsGoogle = self.defaultSearchEngineIsGoogle;
  return icon;
}

- (NSInteger)numberOfLines {
  if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled) {
    return _match.answer_type == omnibox::ANSWER_TYPE_DICTIONARY ? 3 : 1;
  }
  if (_match.answer->second_line().text_fields().empty()) {
    return 1;
  }
  // Answers specify their own limit on the number of lines to show but are
  // additionally capped here at 3 to guard against unreasonable values.
  const SuggestionAnswer::TextField& first_text_field =
      _match.answer->second_line().text_fields()[0];
  if (first_text_field.has_num_lines() && first_text_field.num_lines() > 1)
    return MIN(3, first_text_field.num_lines());
  else
    return 1;
}

- (NSNumber*)suggestionGroupId {
  if (!_match.suggestion_group_id.has_value()) {
    return nil;
  }

  return [NSNumber
      numberWithInt:static_cast<int>(_match.suggestion_group_id.value())];
}

- (NSAttributedString*)text {
  if (self.hasAnswer) {
    return [self answerText];
  } else {
    // The text should be search term (`_match.contents`) for searches,
    // otherwise page title (`_match.description`).
    std::u16string textString =
        self.isURL ? _match.description : _match.contents;

    // Clipboard suggestion "Text you copied" text is stored in description.
    // The content is empty as iOS doesn't access the clipboard when creating
    // the match.
    if (_match.type == AutocompleteMatchType::CLIPBOARD_TEXT ||
        _match.type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
      textString = _match.description;
    }

    NSString* text = base::SysUTF16ToNSString(textString);

    // If for some reason the title is empty, copy the detailText.
    if ([text length] == 0 && [self.detailText length] != 0) {
      text = [self.detailText string];
    }

    const ACMatchClassifications* textClassifications =
        !self.isURL ? &_match.contents_class : &_match.description_class;
    UIColor* suggestionTextColor = SuggestionTextColor();
    UIColor* dimColor = self.incognito ? DimColorIncognito() : DimColor();

    NSAttributedString* attributedText =
        [self attributedStringWithString:text
                         classifications:textClassifications
                               smallFont:NO
                                   color:suggestionTextColor
                                dimColor:dimColor];

    if (self.isTailSuggestion || self.isMultimodal) {
      NSMutableAttributedString* mutableString =
          [[NSMutableAttributedString alloc] init];
      NSAttributedString* tailSuggestPrefix =
          [self attributedStringWithString:@"... "
                           classifications:NULL
                                 smallFont:NO
                                     color:suggestionTextColor
                                  dimColor:dimColor];
      [mutableString appendAttributedString:tailSuggestPrefix];
      [mutableString appendAttributedString:attributedText];
      attributedText = mutableString;
    }
    return attributedText;
  }
}

- (NSAttributedString*)answerText {
  DCHECK(self.hasAnswer);
  UIColor* suggestionTextColor = SuggestionTextColor();
  UIColor* dimColor = self.incognito ? DimColorIncognito() : DimColor();

  if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled) {
    NSMutableAttributedString* result =
        [[NSMutableAttributedString alloc] initWithString:@""];
    NSAttributedString* spacer = [[self class] spacerAttributedString];

    if (_match.answer_type == omnibox::ANSWER_TYPE_DICTIONARY) {
      auto headlineFragments =
          _match.answer_template->answers(0).headline().fragments();

      for (NSInteger i = 0; i < headlineFragments.size(); i++) {
        NSAttributedString* fragmentText;
        // The first fragment has a html bold tag so we skip it and use the
        // match contents instead. The match contents has the first fragment
        // text without the bold tag (eg. match contents : abc , first fragment
        // : ab<b>c</b>).
        // TODO(crbug.com/343706167): Follow up on removing the bold tag from
        // the first fragment.
        if (i == 0) {
          fragmentText =
              [self attributedStringWithString:base::SysUTF16ToNSString(
                                                   _match.contents)
                               classifications:&_match.contents_class
                                     smallFont:NO
                                         color:suggestionTextColor
                                      dimColor:dimColor];
        } else {
          fragmentText =
              [self attributedStringForFragment:headlineFragments[i]
                                          color:SuggestionDetailTextColor()
                         useDeemphasizedStyling:NO];
        }

        [result appendAttributedString:fragmentText];
        [result appendAttributedString:spacer];
      }
    } else {
      auto subheadFragments =
          _match.answer_template->answers(0).subhead().fragments();

      for (auto fragment : subheadFragments) {
        NSAttributedString* fragmentText =
            [self attributedStringForFragment:fragment
                                        color:SuggestionDetailTextColor()
                       useDeemphasizedStyling:NO];
        [result appendAttributedString:fragmentText];
        [result appendAttributedString:spacer];
      }
    }

    // Remove the extra spacer.
    if (result.length > 0) {
      NSRange lastCharacterRange =
          NSMakeRange(result.length - spacer.length, spacer.length);
      [result deleteCharactersInRange:lastCharacterRange];
    }

    return result;
  } else {
    if (!_match.answer->IsExceptedFromLineReversal(_match.answer_type)) {
      return [self attributedStringWithAnswerLine:_match.answer->second_line()
                           useDeemphasizedStyling:NO];
    } else {
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
  }
}

- (NSAttributedString*)omniboxPreviewText {
  return [[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(_match.fill_into_edit)];
}

/// The primary purpose of this list is to omit the "what you typed" types,
/// since those are simply the input in the omnibox and copying the text back to
/// the omnibox would be a noop. However, this list also omits other types that
/// are deprecated or not launched on iOS.
- (BOOL)isAppendable {
  return _match.type == AutocompleteMatchType::BOOKMARK_TITLE ||
         _match.type == AutocompleteMatchType::CALCULATOR ||
         _match.type == AutocompleteMatchType::HISTORY_BODY ||
         _match.type == AutocompleteMatchType::HISTORY_CLUSTER ||
         _match.type == AutocompleteMatchType::HISTORY_KEYWORD ||
         _match.type == AutocompleteMatchType::HISTORY_TITLE ||
         _match.type == AutocompleteMatchType::HISTORY_URL ||
         _match.type == AutocompleteMatchType::NAVSUGGEST ||
         _match.type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED ||
         _match.type == AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED ||
         _match.type == AutocompleteMatchType::SEARCH_HISTORY ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED ||
         _match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL ||
         _match.type == AutocompleteMatchType::STARTER_PACK;
}

- (BOOL)isTabMatch {
  return _match.has_tab_match.value_or(false);
}

- (id<OmniboxPedal>)pedal {
  return self.pedalData;
}

- (UIImage*)matchTypeIcon {
  return GetOmniboxSuggestionIconForAutocompleteMatchType(_match.type);
}

- (NSString*)matchTypeIconAccessibilityIdentifier {
  return base::SysUTF8ToNSString(AutocompleteMatchType::ToString(_match.type));
}

- (BOOL)isMatchTypeSearch {
  return AutocompleteMatch::IsSearchType(_match.type);
}

- (BOOL)isWrapping {
  return self.isMatchTypeSearch && !self.hasAnswer &&
         _match.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
}

- (CrURL*)destinationUrl {
  return [[CrURL alloc] initWithGURL:_match.destination_url];
}

- (const AutocompleteMatch&)autocompleteMatch {
  return _match;
}

#pragma mark tail suggest

- (BOOL)isTailSuggestion {
  return _match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
}

- (NSString*)commonPrefix {
  if (!self.isTailSuggestion) {
    return @"";
  }
  return base::SysUTF16ToNSString(_match.tail_suggest_common_prefix);
}

#pragma mark helpers

/// Create a string to display for an entire answer line.
- (NSAttributedString*)
    attributedStringWithAnswerLine:(const SuggestionAnswer::ImageLine&)line
            useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  NSMutableAttributedString* result =
      [[NSMutableAttributedString alloc] initWithString:@""];

  for (const auto& field : line.text_fields()) {
    [result appendAttributedString:
                [self attributedStringForTextfield:&field
                            useDeemphasizedStyling:useDeemphasizedStyling]];
  }

  return [self addExtraTextFromAnswerLine:line
                       toAttributedString:result
                   useDeemphasizedStyling:useDeemphasizedStyling];
}

/// Adds the `additional_text` and `status_text` from `line` to the given
/// attributed string. This is necessary because answers get their main text
/// from the match contents instead of the ImageLine's text_fields. This is
/// because those fields contain server-provided formatting, which aren't used.
- (NSAttributedString*)
    addExtraTextFromAnswerLine:(const SuggestionAnswer::ImageLine&)line
            toAttributedString:(NSAttributedString*)attributedString
        useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  NSMutableAttributedString* result = [attributedString mutableCopy];

  if (line.additional_text() != nil) {
    [result appendAttributedString:[[self class] spacerAttributedString]];
    NSAttributedString* extra =
        [self attributedStringForTextfield:line.additional_text()
                    useDeemphasizedStyling:useDeemphasizedStyling];
    [result appendAttributedString:extra];
  }

  if (line.status_text() != nil) {
    [result appendAttributedString:[[self class] spacerAttributedString]];
    [result appendAttributedString:
                [self attributedStringForTextfield:line.status_text()
                            useDeemphasizedStyling:useDeemphasizedStyling]];
  }

  return result;
}

/// Create a string to display for a textual part ("textfield") of a suggestion
/// answer.
- (NSAttributedString*)
    attributedStringForTextfield:(const SuggestionAnswer::TextField*)field
          useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  const std::u16string& string = field->text();

  NSString* unescapedString =
      base::SysUTF16ToNSString(base::UnescapeForHTML(string));

  NSDictionary* attributes =
      [self formattingAttributesForSuggestionStyle:field->style()
                            useDeemphasizedStyling:useDeemphasizedStyling];

  return [[NSAttributedString alloc] initWithString:unescapedString
                                         attributes:attributes];
}

#pragma mark FormattedStringFragment styling

// Converts an attributed string fragment proto into an attributedString
- (NSAttributedString*)
    attributedStringForFragment:
        (omnibox::FormattedString::FormattedStringFragment)fragment
                          color:(UIColor*)defaultColor
         useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  NSDictionary* attributes =
      [self formattingAttributesForFragment:fragment
                     useDeemphasizedStyling:useDeemphasizedStyling];

  NSAttributedString* result = [[NSAttributedString alloc]
      initWithString:base::SysUTF8ToNSString(fragment.text())
          attributes:attributes];

  return result;
}

/// Return correct formatting attributes for the fragment proto.
/// `useDeemphasizedStyling` is necessary because some styles (e.g. PRIMARY)
/// should take their color from the surrounding line; they don't have a fixed
/// color.
- (NSDictionary<NSAttributedStringKey, id>*)
    formattingAttributesForFragment:
        (omnibox::FormattedString::FormattedStringFragment)fragment
             useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  UIFontDescriptor* defaultFontDescriptor =
      useDeemphasizedStyling
          ? [[UIFontDescriptor
                preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline]
                fontDescriptorWithSymbolicTraits:
                    UIFontDescriptorTraitTightLeading]
          : [UIFontDescriptor
                preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  UIColor* defaultColor = useDeemphasizedStyling ? SuggestionDetailTextColor()
                                                 : SuggestionTextColor();

  omnibox::FormattedString::ColorType color = fragment.color();
  switch (color) {
    case omnibox::FormattedString::COLOR_ON_SURFACE_POSITIVE:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : [UIColor colorNamed:kGreenColor],
      };
    case omnibox::FormattedString::COLOR_ON_SURFACE_NEGATIVE:
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : [UIColor colorNamed:kRedColor],
      };
    case omnibox::FormattedString::COLOR_PRIMARY: {
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
    default:
      BOOL isFinanceDetailText =
          _match.answer_type == omnibox::ANSWER_TYPE_FINANCE &&
          useDeemphasizedStyling;
      return @{
        NSFontAttributeName : [UIFont fontWithDescriptor:defaultFontDescriptor
                                                    size:0],
        NSForegroundColorAttributeName : isFinanceDetailText ? UIColor.grayColor
                                                             : defaultColor,
      };
  }
}

/// Return correct formatting attributes for the given style.
/// `useDeemphasizedStyling` is necessary because some styles (e.g. SUPERIOR)
/// should take their color from the surrounding line; they don't have a fixed
/// color.
- (NSDictionary<NSAttributedStringKey, id>*)
    formattingAttributesForSuggestionStyle:(SuggestionAnswer::TextStyle)style
                    useDeemphasizedStyling:(BOOL)useDeemphasizedStyling {
  UIFontDescriptor* defaultFontDescriptor =
      useDeemphasizedStyling
          ? [[UIFontDescriptor
                preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline]
                fontDescriptorWithSymbolicTraits:
                    UIFontDescriptorTraitTightLeading]
          : [UIFontDescriptor
                preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  UIColor* defaultColor = useDeemphasizedStyling ? SuggestionDetailTextColor()
                                                 : SuggestionTextColor();

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

/// Create a formatted string given text and classifications.
- (NSMutableAttributedString*)
    attributedStringWithString:(NSString*)text
               classifications:(const ACMatchClassifications*)classifications
                     smallFont:(BOOL)smallFont
                         color:(UIColor*)defaultColor
                      dimColor:(UIColor*)dimColor {
  if (text == nil)
    return nil;

  UIFont* fontRef;
  fontRef = smallFont
                ? [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
                : [UIFont preferredFontForTextStyle:UIFontTextStyleBody];

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
    UIFontDescriptor* fontDescriptor = fontRef.fontDescriptor;
    UIFontDescriptor* boldFontDescriptor = [fontDescriptor
        fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
    boldFontRef = [UIFont fontWithDescriptor:boldFontDescriptor size:0];

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
