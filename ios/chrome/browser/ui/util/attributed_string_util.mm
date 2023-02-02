// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/attributed_string_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
