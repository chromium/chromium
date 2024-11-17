// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_ITEM_H_

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

@protocol StandaloneModuleDelegate;

// Item containing the configurations for standalone-type Magic Stack Module
// view.
@interface StandaloneModuleItem : MagicStackModule

// Delegate to relay user actions.
@property(nonatomic, weak) id<StandaloneModuleDelegate> standaloneDelegate;

// Favicon image for the module.
@property(nonatomic, strong) UIImage* faviconImage;

// Fallack image for the module for when faviconImage is unset.
@property(nonatomic, strong) UIImage* fallbackSymbolImage;

// Title text for the module.
@property(nonatomic, copy) NSString* titleText;

// Body text for the module.
@property(nonatomic, copy) NSString* bodyText;

// Text for the button for the module.
@property(nonatomic, copy) NSString* buttonText;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_ITEM_H_
