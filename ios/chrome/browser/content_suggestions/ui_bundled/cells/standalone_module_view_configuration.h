// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_STANDALONE_MODULE_VIEW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_STANDALONE_MODULE_VIEW_CONFIGURATION_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/ui/magic_stack_module.h"

// Item containing the configurations for standalone-type Magic Stack Module
// view.
@interface StandaloneModuleViewConfiguration : MagicStackModule

// Product image for the module
@property(nonatomic, strong) UIImage* productImage;

// Favicon image for the module. Appears as a small stamp when `productImage` is
// set. Appears as a larger icon when `productImage` is not set.
@property(nonatomic, strong) UIImage* faviconImage;

// Fallback image for the module for when `productImage` and `faviconImage` are
// unset.
@property(nonatomic, strong) UIImage* fallbackSymbolImage;

// Title text for the module.
@property(nonatomic, copy) NSString* titleText;

// Body text for the module.
@property(nonatomic, copy) NSString* bodyText;

// Text for the button for the module.
@property(nonatomic, copy) NSString* buttonText;

// Accessibility identifier for the module.
@property(nonatomic, copy) NSString* accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_STANDALONE_MODULE_VIEW_CONFIGURATION_H_
