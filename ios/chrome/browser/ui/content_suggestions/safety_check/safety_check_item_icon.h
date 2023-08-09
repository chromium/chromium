// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_ICON_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_ICON_H_

#import <UIKit/UIKit.h>

// A view which contains an icon for a Safety Check item.
@interface SafetyCheckItemIcon : UIView

// Instantiates a `SafetyCheckItemIcon` given a `defaultSymbolName`.
//
// `compactLayout` determines if the icon should be shown in a smaller, compact
// size.
//
// `inSquare` determines if the icon should be shown with a square enclosure
// surrounding it.
- (instancetype)initWithDefaultSymbol:(NSString*)defaultSymbolName
                        compactLayout:(BOOL)compactLayout
                             inSquare:(BOOL)inSquare;

// Instantiates a `SafetyCheckItemIcon` given a `customSymbolName`.
//
// `compactLayout` determines if the icon should be shown in a smaller, compact
// size.
//
// `inSquare` determines if the icon should be shown with a square enclosure
// surrounding it.
- (instancetype)initWithCustomSymbol:(NSString*)customSymbolName
                       compactLayout:(BOOL)compactLayout
                            inSquare:(BOOL)inSquare;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_ICON_H_
