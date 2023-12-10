// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_PROTECTED_H_

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "ios/chrome/browser/ui/menu/menu_action_type.h"

@interface ActionFactory (Protected)

// Creates a UIAction instance configured with the given `title` and `image`.
// Upon execution, the action's `type` will be recorded and the `block` will be
// run.
- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_PROTECTED_H_
