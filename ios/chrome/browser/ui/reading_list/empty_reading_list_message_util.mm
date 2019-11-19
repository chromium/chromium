// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/empty_reading_list_message_util.h"

#include "base/logging.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Images name.
NSString* const kToolsIcon = @"reading_list_tools_icon";

// Tag in string.
NSString* const kOpenShareMarker = @"SHARE_OPENING_ICON";
NSString* const kReadLaterTextMarker = @"READ_LATER_TEXT";

// Background view constants.
const CGFloat kLineSpacing = 4;

// Returns the font to use for the message text.
UIFont* GetMessageFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
}

// Returns the attributes to use for the message text.
NSMutableDictionary* GetMessageAttributes() {
  NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
  UIFont* font = GetMessageFont();
  attributes[NSFontAttributeName] = font;
  attributes[NSForegroundColorAttributeName] =
      [UIColor colorNamed:kTextSecondaryColor];
  NSMutableParagraphStyle* paragraph_style =
      [[NSMutableParagraphStyle alloc] init];
  paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
  paragraph_style.alignment = NSTextAlignmentCenter;
  // If the line wrapping occurs that one of the icons is the first character on
  // a new line, the default line spacing will result in uneven line heights.
  // Manually setting the line spacing here prevents that from occurring.
  paragraph_style.lineSpacing = kLineSpacing;
  attributes[NSParagraphStyleAttributeName] = paragraph_style;
  return attributes;
}

// Returns the attributes to use for the instructions on how to reach the "Read
// Later" option.
NSMutableDictionary* GetInstructionAttributes() {
  NSMutableDictionary* attributes = GetMessageAttributes();
  attributes[NSFontAttributeName] =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  return attributes;
}

// Returns the "Read Later" text to appear at the end of the string, with
// correct styling.
NSAttributedString* GetReadLaterString() {
  NSString* read_later_text =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_READING_LIST_ACTION);
  return [[NSAttributedString alloc] initWithString:read_later_text
                                         attributes:GetInstructionAttributes()];
}

// Appends the tools icon to |text|.  Spacer text that is added by this function
// is formatted with |attributes|.
void AppendToolsIcon(NSMutableAttributedString* text,
                     NSDictionary* attributes) {
  if (@available(iOS 13, *)) {
  } else {
    // Add a zero width space to set the attributes for the image.
    NSAttributedString* spacer =
        [[NSAttributedString alloc] initWithString:@"\u200B"
                                        attributes:attributes];
    [text appendAttributedString:spacer];
  }

  // The icon bounds must be offset to be vertically centered with the message
  // text.
  UIImage* icon = [UIImage imageNamed:kToolsIcon];
  CGRect icon_bounds = CGRectZero;
  icon_bounds.size = icon.size;
  icon_bounds.origin.y = (GetMessageFont().xHeight - icon.size.height) / 2.0;

  // Attach the icon image.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  if (@available(iOS 13, *)) {
    attachment.image =
        [icon imageWithTintColor:attributes[NSForegroundColorAttributeName]];
  } else {
    attachment.image =
        [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  attachment.bounds = icon_bounds;
  NSAttributedString* attachment_string =
      [NSAttributedString attributedStringWithAttachment:attachment];
  [text appendAttributedString:attachment_string];
}

// Returns the string to use to describe the buttons needed to access the "Read
// Later" option.
NSAttributedString* GetInstructionIconString() {
  NSDictionary* attributes = GetInstructionAttributes();
  NSMutableAttributedString* icon_string =
      [[NSMutableAttributedString alloc] init];
  AppendToolsIcon(icon_string, attributes);
  return icon_string;
}

// Returns the icon string in an accessible format.
NSAttributedString* GetAccessibleInstructionIconString() {
  NSDictionary* attributes = GetInstructionAttributes();
  NSMutableAttributedString* icon_string =
      [[NSMutableAttributedString alloc] initWithString:@":"
                                             attributes:attributes];
  NSString* tools_text = [NSString
      stringWithFormat:@"%@, ",
                       l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS)];
  [icon_string appendAttributedString:[[NSAttributedString alloc]
                                          initWithString:tools_text
                                              attributes:attributes]];
  return icon_string;
}

// Returns the attributed text to use for the message.  If |use_icons| is true,
// icon images are added to the text; otherwise accessible text versions of the
// instructions are used.
NSAttributedString* GetReadingListEmptyMessage(bool use_icons) {
  NSString* raw_text =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_EMPTY_MESSAGE);
  NSMutableAttributedString* message =
      [[NSMutableAttributedString alloc] initWithString:raw_text
                                             attributes:GetMessageAttributes()];
  NSAttributedString* instruction_icon_string =
      use_icons ? GetInstructionIconString()
                : GetAccessibleInstructionIconString();
  NSAttributedString* read_later_string = GetReadLaterString();
  // Two replacements must be made in the text:
  // - kOpenShareMarker should be replaced with |instruction_icon_string|
  // - kReadLaterTextMarker should be replaced with |read_later_text|
  NSRange icon_range = [message.string rangeOfString:kOpenShareMarker];
  DCHECK(icon_range.location != NSNotFound);
  [message replaceCharactersInRange:icon_range
               withAttributedString:instruction_icon_string];

  NSRange read_later_range =
      [message.string rangeOfString:kReadLaterTextMarker];
  DCHECK(read_later_range.location != NSNotFound);
  [message replaceCharactersInRange:read_later_range
               withAttributedString:read_later_string];
  return message;
}
}  // namespace

NSAttributedString* GetReadingListEmptyMessage() {
  return GetReadingListEmptyMessage(true);
}

NSString* GetReadingListEmptyMessageA11yLabel() {
  return GetReadingListEmptyMessage(false).string;
}
