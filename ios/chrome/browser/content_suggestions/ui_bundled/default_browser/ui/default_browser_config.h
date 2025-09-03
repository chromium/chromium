// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_CONFIG_H_

@protocol DefaultBrowserCommands;

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module.h"

// Item containing the configurations for the Default Browser view.
@interface DefaultBrowserConfig : MagicStackModule

// Command handler for user actions.
@property(nonatomic, weak) id<DefaultBrowserCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_CONFIG_H_
