// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTAINER_EDIT_MENU_BUILDER_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTAINER_EDIT_MENU_BUILDER_H_

#import <UIKit/UIKit.h>

// A protocol to customize the edit menu.
@protocol EditMenuBuilder <NSObject>

// Customizes the edit menu.
- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTAINER_EDIT_MENU_BUILDER_H_
