// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_VIEW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_VIEW_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration for the `IconView`.
@interface IconViewConfiguration : NSObject

// Content properties.
// YES if this icon should configure itself in a smaller, compact
// size.
@property(nonatomic, assign) BOOL compactLayout;
// YES if this icon should place itself within a square enclosure.
@property(nonatomic, assign) BOOL inSquare;

// Symbol properties.
// The symbol name for the icon.
@property(nonatomic, copy) NSString* symbol;
// The color palette of the icon.
@property(nonatomic, copy) NSArray<UIColor*>* symbolColorPalette;
// The background color of the icon.
@property(nonatomic, strong) UIColor* symbolBackgroundColor;
// The width of the symbol.
@property(nonatomic, assign) CGFloat symbolWidth;
// YES if `symbol` is a default symbol name. (NO if `symbol` is a custom
// symbol name.)
@property(nonatomic, assign) BOOL defaultSymbol;

// Convenience initializer to create a configuration with a `symbol`.
+ (instancetype)configurationWithSymbolNamed:(NSString*)symbol;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_VIEW_CONFIGURATION_H_
