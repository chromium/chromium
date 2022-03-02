// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/string_util.h"

#import <UIKit/UIKit.h>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
typedef BOOL (^ArrayFilterProcedure)(id object, NSUInteger index, BOOL* stop);
typedef NSString* (^SubstringExtractionProcedure)(NSUInteger);
NSString* const kBeginLinkTag = @"BEGIN_LINK[ \t]*";
NSString* const kEndLinkTag = @"[ \t]*END_LINK";
NSString* const kBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kEndBoldTag = @"[ \t]*END_BOLD";
}

StringWithTags::StringWithTags() = default;

StringWithTags::StringWithTags(NSString* string, std::vector<NSRange> ranges)
    : string([string copy]), ranges(ranges) {}

StringWithTags::StringWithTags(const StringWithTags& other) = default;

StringWithTags& StringWithTags::operator=(const StringWithTags& other) =
    default;

StringWithTags::StringWithTags(StringWithTags&& other) = default;

StringWithTags& StringWithTags::operator=(StringWithTags&& other) = default;

StringWithTags::~StringWithTags() = default;

StringWithTags ParseStringWithLinks(NSString* text) {
  return ParseStringWithTags(text, kBeginLinkTag, kEndLinkTag);
}

NSAttributedString* AttributedStringFromStringWithLink(
    NSString* text,
    NSDictionary* text_attributes,
    NSDictionary* link_attributes) {
  StringWithTag parsed_string =
      ParseStringWithTag(text, kBeginLinkTag, kEndLinkTag);
  NSMutableAttributedString* attributed_string =
      [[NSMutableAttributedString alloc] initWithString:parsed_string.string
                                             attributes:text_attributes];

  DCHECK(parsed_string.range.location != NSNotFound);

  if (link_attributes != nil) {
    [attributed_string addAttributes:link_attributes range:parsed_string.range];
  }

  return attributed_string;
}

StringWithTag ParseStringWithTag(NSString* text,
                                 NSString* begin_tag,
                                 NSString* end_tag) {
  const StringWithTags parsed_string =
      ParseStringWithTags(text, begin_tag, end_tag);

  DCHECK_LE(parsed_string.ranges.size(), 1u);
  return StringWithTag{parsed_string.string, parsed_string.ranges.empty()
                                                 ? NSRange{NSNotFound, 0}
                                                 : parsed_string.ranges[0]};
}

StringWithTags ParseStringWithTags(NSString* text,
                                   NSString* begin_tag,
                                   NSString* end_tag) {
  NSMutableString* out_text = nil;
  std::vector<NSRange> tag_ranges;

  NSRange text_range{0, text.length};
  do {
    // Find the next |begin_tag| in |text_range|.
    const NSRange begin_range = [text rangeOfString:begin_tag
                                            options:NSRegularExpressionSearch
                                              range:text_range];

    // If no |begin_tag| is found, then there is no substitutions remainining.
    if (begin_range.length == 0)
      break;

    // Find the next |end_tag| after the recently found |begin_tag|.
    const NSUInteger after_begin_pos = NSMaxRange(begin_range);
    const NSRange after_begin_range{
        after_begin_pos,
        text_range.length - (after_begin_pos - text_range.location)};
    const NSRange end_range = [text rangeOfString:end_tag
                                          options:NSRegularExpressionSearch
                                            range:after_begin_range];

    // If no |end_tag| is found, then there is no substitutions remaining.
    if (end_range.length == 0)
      break;

    if (!out_text)
      out_text = [[NSMutableString alloc] initWithCapacity:text.length];

    const NSUInteger after_end_pos = NSMaxRange(end_range);
    [out_text
        appendString:[text
                         substringWithRange:NSRange{text_range.location,
                                                    begin_range.location -
                                                        text_range.location}]];
    [out_text
        appendString:[text substringWithRange:NSRange{after_begin_pos,
                                                      end_range.location -
                                                          after_begin_pos}]];

    const NSUInteger tag_length = end_range.location - after_begin_pos;
    tag_ranges.push_back(NSRange{out_text.length - tag_length, tag_length});

    text_range =
        NSRange{after_end_pos,
                text_range.length - (after_end_pos - text_range.location)};
  } while (text_range.length != 0);

  if (!out_text) {
    DCHECK(tag_ranges.empty());
    return StringWithTags(text, {});
  }

  // Append any remaining text without tags.
  if (text_range.length != 0)
    [out_text appendString:[text substringWithRange:text_range]];

  return StringWithTags(out_text, tag_ranges);
}

// Ranges of unicode codepage containing drawing characters.
// 2190—21FF Arrows
// 2200—22FF Mathematical Operators
// 2300—23FF Miscellaneous Technical
// 2400—243F Control Pictures
// 2440—245F Optical Character Recognition
// 2460—24FF Enclosed Alphanumerics
// 2500—257F Box Drawing
// 2580—259F Block Elements
// 25A0—25FF Geometric Shapes
// 2600—26FF Miscellaneous Symbols
// 2700—27BF Dingbats
// 27C0—27EF Miscellaneous Mathematical Symbols-A
// 27F0—27FF Supplemental Arrows-A
// 2900—297F Supplemental Arrows-B
// 2980—29FF Miscellaneous Mathematical Symbols-B
// 2A00—2AFF Supplemental Mathematical Operators
// 2B00—2BFF Miscellaneous Symbols and Arrows
// The section 2800—28FF Braille Patterns must be preserved.
// The list of characters that must be deleted from the selection.
NSCharacterSet* GraphicCharactersSet() {
  static NSMutableCharacterSet* graphicalCharsSet;
  static dispatch_once_t dispatch_once_token;
  dispatch_once(&dispatch_once_token, ^{
    graphicalCharsSet = [[NSMutableCharacterSet alloc] init];
    NSRange graphicalCharsFirstRange = NSMakeRange(0x2190, 0x2800 - 0x2190);
    NSRange graphicalCharsSecondRange = NSMakeRange(0x2900, 0x2c00 - 0x2900);
    [graphicalCharsSet addCharactersInRange:graphicalCharsFirstRange];
    [graphicalCharsSet addCharactersInRange:graphicalCharsSecondRange];
  });
  return graphicalCharsSet;
}

NSString* CleanNSStringForDisplay(NSString* dirty, BOOL removeGraphicChars) {
  NSCharacterSet* wspace = [NSCharacterSet whitespaceAndNewlineCharacterSet];
  NSString* cleanString = dirty;
  if (removeGraphicChars) {
    cleanString = [[cleanString
        componentsSeparatedByCharactersInSet:GraphicCharactersSet()]
        componentsJoinedByString:@" "];
  }
  NSMutableArray* spaceSeparatedComponents =
      [[cleanString componentsSeparatedByCharactersInSet:wspace] mutableCopy];
  ArrayFilterProcedure filter = ^(id object, NSUInteger index, BOOL* stop) {
    return [object isEqualToString:@""];
  };
  [spaceSeparatedComponents
      removeObjectsAtIndexes:[spaceSeparatedComponents
                                 indexesOfObjectsPassingTest:filter]];
  cleanString = [spaceSeparatedComponents componentsJoinedByString:@" "];
  return cleanString;
}

std::string CleanStringForDisplay(const std::string& dirty,
                                  BOOL removeGraphicChars) {
  return base::SysNSStringToUTF8(CleanNSStringForDisplay(
      base::SysUTF8ToNSString(dirty), removeGraphicChars));
}

NSString* SubstringOfWidth(NSString* string,
                           NSDictionary* attributes,
                           CGFloat targetWidth,
                           BOOL trailing) {
  if (![string length])
    return nil;

  UIFont* font = [attributes objectForKey:NSFontAttributeName];
  DCHECK(font);

  // Function to get the correct substring while insulating against
  // length overrun/underrun.
  SubstringExtractionProcedure getSubstring;
  if (trailing) {
    getSubstring = [^NSString*(NSUInteger chars) {
      NSUInteger length = [string length];
      return [string substringFromIndex:length - MIN(length, chars)];
    } copy];
  } else {
    getSubstring = [^NSString*(NSUInteger chars) {
      return [string substringToIndex:MIN(chars, [string length])];
    } copy];
  }

  // Guess at the number of characters that will fit, assuming
  // the font's x-height is about 25% wider than an average character (25%
  // value was determined experimentally).
  NSUInteger characters =
      MIN(targetWidth / (font.xHeight * 0.8), [string length]);
  NSInteger increment = 1;
  NSString* substring = getSubstring(characters);
  CGFloat prevWidth = [substring sizeWithAttributes:attributes].width;
  do {
    characters += increment;
    substring = getSubstring(characters);
    CGFloat thisWidth = [substring sizeWithAttributes:attributes].width;
    if (prevWidth > targetWidth) {
      if (thisWidth <= targetWidth)
        break;  // Shrinking the string, found the right size.
      else
        increment = -1;  // Shrink the string
    } else if (prevWidth < targetWidth) {
      if (thisWidth < targetWidth)
        increment = 1;  // Grow the string
      else {
        substring = getSubstring(characters - increment);
        break;  // Growing the string, found the right size.
      }
    }
    prevWidth = thisWidth;
  } while (characters > 0 && characters < [string length]);

  return substring;
}

CGRect TextViewLinkBound(UITextView* text_view, NSRange character_range) {
  // Calculate UITextRange with NSRange.
  UITextPosition* beginning = text_view.beginningOfDocument;
  UITextPosition* start =
      [text_view positionFromPosition:beginning
                               offset:character_range.location];
  UITextPosition* end = [text_view positionFromPosition:start
                                                 offset:character_range.length];

  CGRect rect = CGRectNull;
  // Returns CGRectNull if there is a nil text position.
  if (start && end) {
    UITextRange* text_range = [text_view textRangeFromPosition:start
                                                    toPosition:end];

    NSArray* selection_rects = [text_view selectionRectsForRange:text_range];

    for (UITextSelectionRect* selection_rect in selection_rects) {
      rect = CGRectUnion(rect, selection_rect.rect);
    }
  }

  return rect;
}

NSAttributedString* PutBoldPartInString(NSString* string,
                                        UIFontTextStyle font_style) {
  UIFontDescriptor* default_descriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:font_style];
  UIFontDescriptor* bold_descriptor =
      [[UIFontDescriptor preferredFontDescriptorWithTextStyle:font_style]
          fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  StringWithTag parsed_string =
      ParseStringWithTag(string, kBeginBoldTag, kEndBoldTag);

  NSMutableAttributedString* attributed_string =
      [[NSMutableAttributedString alloc] initWithString:parsed_string.string];
  [attributed_string addAttribute:NSFontAttributeName
                            value:[UIFont fontWithDescriptor:default_descriptor
                                                        size:0.0]
                            range:NSMakeRange(0, parsed_string.string.length)];

  [attributed_string addAttribute:NSFontAttributeName
                            value:[UIFont fontWithDescriptor:bold_descriptor
                                                        size:0.0]
                            range:parsed_string.range];

  return attributed_string;
}
