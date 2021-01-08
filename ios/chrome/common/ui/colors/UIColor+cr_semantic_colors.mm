// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIColor (CRSemanticColors)

#pragma mark - System Background Colors

+ (UIColor*)cr_systemBackgroundColor {
  if (@available(iOS 13, *)) {
    return UIColor.systemBackgroundColor;
  }
  return UIColor.whiteColor;
}

+ (UIColor*)cr_secondarySystemBackgroundColor {
  if (@available(iOS 13, *)) {
    return UIColor.secondarySystemBackgroundColor;
  }
  // This is the value for secondarySystemBackgroundColor in light mode.
  return [UIColor colorWithRed:244 / (CGFloat)0xFF
                         green:244 / (CGFloat)0xFF
                          blue:248 / (CGFloat)0xFF
                         alpha:1];
}

#pragma mark - System Grouped Background Colors

+ (UIColor*)cr_systemGroupedBackgroundColor {
  if (@available(iOS 13, *)) {
    return UIColor.systemGroupedBackgroundColor;
  }
  return UIColor.groupTableViewBackgroundColor;
}

+ (UIColor*)cr_secondarySystemGroupedBackgroundColor {
  if (@available(iOS 13, *)) {
    return UIColor.secondarySystemGroupedBackgroundColor;
  }
  return UIColor.whiteColor;
}

#pragma mark - Label Colors

+ (UIColor*)cr_labelColor {
  if (@available(iOS 13, *)) {
    return UIColor.labelColor;
  }
  return UIColor.blackColor;
}

+ (UIColor*)cr_secondaryLabelColor {
  if (@available(iOS 13, *)) {
    return UIColor.secondaryLabelColor;
  }
  // This is the value for UIColor.secondaryLabelColor in light mode.
  return [UIColor colorWithRed:0x3C / (CGFloat)0xFF
                         green:0x3C / (CGFloat)0xFF
                          blue:0x43 / (CGFloat)0xFF
                         alpha:0.6];
}

#pragma mark - Separator Colors

+ (UIColor*)cr_separatorColor {
  if (@available(iOS 13, *)) {
    return UIColor.separatorColor;
  }
  // This is the value for separatorColor in light mode.
  return [UIColor colorWithRed:0x3C / (CGFloat)0xFF
                         green:0x3C / (CGFloat)0xFF
                          blue:0x43 / (CGFloat)0xFF
                         alpha:0.6];
}

+ (UIColor*)cr_opaqueSeparatorColor {
  if (@available(iOS 13, *)) {
    return UIColor.opaqueSeparatorColor;
  }
  // This is the value for opaqueSeparatorColor in light mode.
  return [UIColor colorWithRed:0xC7 / (CGFloat)0xFF
                         green:0xC7 / (CGFloat)0xFF
                          blue:0xC7 / (CGFloat)0xFF
                         alpha:1];
}

#pragma mark - Gray Colors

+ (UIColor*)cr_systemGray2Color {
  if (@available(iOS 13, *)) {
    return UIColor.systemGray2Color;
  }
  // This is the value for systemGray2Color in light mode.
  return [UIColor colorWithRed:174 / (CGFloat)0xFF
                         green:174 / (CGFloat)0xFF
                          blue:178 / (CGFloat)0xFF
                         alpha:1];
}

+ (UIColor*)cr_systemGray3Color {
  if (@available(iOS 13, *)) {
    return UIColor.systemGray3Color;
  }
  // This is the value for systemGray3Color in light mode.
  return [UIColor colorWithRed:199 / (CGFloat)0xFF
                         green:199 / (CGFloat)0xFF
                          blue:204 / (CGFloat)0xFF
                         alpha:1];
}

+ (UIColor*)cr_systemGray4Color {
  if (@available(iOS 13, *)) {
    return UIColor.systemGray4Color;
  }
  // This is the value for systemGray4Color in light mode.
  return [UIColor colorWithRed:209 / (CGFloat)0xFF
                         green:209 / (CGFloat)0xFF
                          blue:214 / (CGFloat)0xFF
                         alpha:1];
}

+ (UIColor*)cr_systemGray5Color {
  if (@available(iOS 13, *)) {
    return UIColor.systemGray5Color;
  }
  // This is the value for systemGray5Color in light mode.
  return [UIColor colorWithRed:229 / (CGFloat)0xFF
                         green:229 / (CGFloat)0xFF
                          blue:234 / (CGFloat)0xFF
                         alpha:1];
}

+ (UIColor*)cr_systemGray6Color {
  if (@available(iOS 13, *)) {
    return UIColor.systemGray6Color;
  }
  // This is the value for systemGray6Color in light mode.
  return [UIColor colorWithRed:242 / (CGFloat)0xFF
                         green:242 / (CGFloat)0xFF
                          blue:247 / (CGFloat)0xFF
                         alpha:1];
}

@end
