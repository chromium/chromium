// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_COLORFUL_SYMBOL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_COLORFUL_SYMBOL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

// A content configuration for a symbol image view with a colorful background.
@interface ColorfulSymbolContentConfiguration
    : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The symbol image to be displayed.
@property(nonatomic, strong) UIImage* symbolImage;

// The background color of the symbol.
@property(nonatomic, strong) UIColor* symbolBackgroundColor;

// The tint color of the symbol.
@property(nonatomic, strong) UIColor* symbolTintColor;

// LINT.ThenChange(colorful_symbol_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_COLORFUL_SYMBOL_CONTENT_CONFIGURATION_H_
