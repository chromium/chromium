// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ICON_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ICON_VIEW_H_

#import <UIKit/UIKit.h>

// A view which contains an icon for a Safety Check item.
@interface IconView : UIView

// Instantiates an `IconView` given a `defaultSymbolName`.
//
// `symbolColorPalette` determines the color palette of the icon itself.
//
// `symbolBackgroundColor` determines the background color of the icon.
//
// `symbolWidth` determines the width of the icon.
//
// `compactLayout` determines if the icon should be shown in a smaller, compact
// size.
//
// `inSquare` determines if the icon should be shown with a square enclosure
// surrounding it.
- (instancetype)initWithDefaultSymbol:(NSString*)defaultSymbolName
                   symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
                symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                          symbolWidth:(CGFloat)symbolWidth
                        compactLayout:(BOOL)compactLayout
                             inSquare:(BOOL)inSquare;

// Instantiates an `IconView` given a `defaultSymbolName`.
//
// `symbolWidth` determines the width of the icon.
//
// `compactLayout` determines if the icon should be shown in a smaller, compact
// size.
//
// `inSquare` determines if the icon should be shown with a square enclosure
// surrounding it.
- (instancetype)initWithDefaultSymbol:(NSString*)defaultSymbolName
                          symbolWidth:(CGFloat)symbolWidth
                        compactLayout:(BOOL)compactLayout
                             inSquare:(BOOL)inSquare;

// Instantiates an `IconView` given a `customSymbolName`.
//
// `symbolColorPalette` determines the color palette of the icon itself.
//
// `symbolBackgroundColor` determines the background color of the icon.
//
// `symbolWidth` determines the width of the icon.
//
// `compactLayout` determines if the icon should be shown in a smaller, compact
// size.
//
// `inSquare` determines if the icon should be shown with a square enclosure
// surrounding it.
- (instancetype)initWithCustomSymbol:(NSString*)customSymbolName
                  symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
               symbolBackgroundColor:(UIColor*)symbolBackgroundColor
                         symbolWidth:(CGFloat)symbolWidth
                       compactLayout:(BOOL)compactLayout
                            inSquare:(BOOL)inSquare;

// Instantiates an `IconView` given a `customSymbolName`.
//
// `symbolWidth` determines the width of the icon.
//
// `compactLayout` determines if the icon should be shown in a smaller, compact
// size.
//
// `inSquare` determines if the icon should be shown with a square enclosure
// surrounding it.
- (instancetype)initWithCustomSymbol:(NSString*)customSymbolName
                         symbolWidth:(CGFloat)symbolWidth
                       compactLayout:(BOOL)compactLayout
                            inSquare:(BOOL)inSquare;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ICON_VIEW_H_
