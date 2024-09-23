// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_MENU_BUILDER_H_
#define IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_MENU_BUILDER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Builds a non-contextual menu of keyboard commands during application launch.
@interface MenuBuilder : NSObject

// Configures the builder with the relevant keyboard commands.
+ (void)buildMainMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_MENU_BUILDER_H_
