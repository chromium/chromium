// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/string_util.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"

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
    // Find the next `begin_tag` in `text_range`.
    const NSRange begin_range = [text rangeOfString:begin_tag
                                            options:NSRegularExpressionSearch
                                              range:text_range];

    // If no `begin_tag` is found, then there is no substitutions remainining.
    if (begin_range.length == 0)
      break;

    // Find the next `end_tag` after the recently found `begin_tag`.
    const NSUInteger after_begin_pos = NSMaxRange(begin_range);
    const NSRange after_begin_range{
        after_begin_pos,
        text_range.length - (after_begin_pos - text_range.location)};
    const NSRange end_range = [text rangeOfString:end_tag
                                          options:NSRegularExpressionSearch
                                            range:after_begin_range];

    // If no `end_tag` is found, then there is no substitutions remaining.
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
