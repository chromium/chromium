// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_COLORFUL_SYMBOL_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_COLORFUL_SYMBOL_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

@class ColorfulSymbolContentConfiguration;

// A content view for a symbol image view with a colorful background.
@interface ColorfulSymbolContentView : UIView <UIContentView>

- (instancetype)initWithConfiguration:
    (ColorfulSymbolContentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_COLORFUL_SYMBOL_CONTENT_VIEW_H_
