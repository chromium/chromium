// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_COLORFUL_BACKGROUND_SYMBOL_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_COLORFUL_BACKGROUND_SYMBOL_VIEW_H_

#import <UIKit/UIKit.h>

// A view used to display a symbol with a colorful background.
@interface ColorfulBackgroundSymbolView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The color of the border. If nil, then there is no border (default is nil).
@property(nonatomic, strong) UIColor* borderColor;

// Sets the tint color of the symbol (default is white, setting it to nil also
// make it white).
@property(nonatomic, strong) UIColor* symbolTintColor;

// Sets the symbol to the symbol named `symbolName`, `systemSymbol` is used to
// check if it is a symbol provided by the system or not. When using this
// method, the default size is used.
- (void)setSymbolName:(NSString*)symbolName systemSymbol:(BOOL)systemSymbol;

// Sets the symbol.
// @discussion
// Use this setter when your symbol needs to be of a custom size.
- (void)setSymbol:(UIImage*)symbol;

// Resets all the properties of the view, making it ready to be used again.
- (void)resetView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_COLORFUL_BACKGROUND_SYMBOL_VIEW_H_
