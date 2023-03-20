// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_MENU_BUILDER_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_MENU_BUILDER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Builds a non-contextual menu of keyboard commands during application launch.
//
// Note: The OS API to build the menu is only available in iOS 15+. On iOS 14,
// each view controller returns key commands via -keyCommands. These are all
// bundled together in the iOS 9-to-14-style menu.
API_AVAILABLE(ios(15.0))
@interface MenuBuilder : NSObject

// Configures the builder with the relevant keyboard commands.
+ (void)buildMainMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_MENU_BUILDER_H_
