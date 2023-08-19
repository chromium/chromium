// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/attributed_string_util.h"

NSAttributedString* AttributedStringCopyWithAttributes(
    NSAttributedString* attributedString,
    NSLineBreakMode lineBreakMode,
    NSTextAlignment textAlignment,
    BOOL forceLeftToRight) {
  NSMutableAttributedString* textCopy = [attributedString mutableCopy];
  NSMutableParagraphStyle* textStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  textStyle.lineBreakMode = lineBreakMode;
  textStyle.alignment = textAlignment;
  textStyle.lineBreakStrategy = NSLineBreakStrategyHangulWordPriority;
  if (forceLeftToRight) {
    textStyle.baseWritingDirection = NSWritingDirectionLeftToRight;
  }
  [textCopy addAttribute:NSParagraphStyleAttributeName
                   value:textStyle
                   range:NSMakeRange(0, attributedString.length)];
  return textCopy;
}

NSInteger NumberOfLinesOfAttributedString(NSAttributedString* attributedString,
                                          CGFloat limitedWidth) {
  NSAttributedString* wrappingString = AttributedStringCopyWithAttributes(
      attributedString, NSLineBreakByWordWrapping, NSTextAlignmentNatural, NO);
  const CGSize wrappingStringSize =
      [wrappingString boundingRectWithSize:CGSizeMake(limitedWidth, FLT_MAX)
                                   options:NSStringDrawingUsesLineFragmentOrigin
                                   context:nil]
          .size;
  const NSInteger numberOfLines =
      round(wrappingStringSize.height / wrappingString.size.height);
  return numberOfLines;
}

NSAttributedString* NSAttributedStringFromUILabel(UILabel* label) {
  NSShadow* shadow = [[NSShadow alloc] init];
  shadow.shadowColor = label.shadowColor;
  shadow.shadowOffset = label.shadowOffset;
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = label.textAlignment;
  paragraphStyle.lineBreakMode = label.lineBreakMode;
  NSAttributedString* attributedText = [[NSAttributedString alloc]
      initWithString:label.text
          attributes:@{
            NSFontAttributeName : label.font,
            NSForegroundColorAttributeName : label.textColor,
            NSShadowAttributeName : shadow,
            NSParagraphStyleAttributeName : paragraphStyle
          }];
  return attributedText;
}
