// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_EDIT_MENU_ALERT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_EDIT_MENU_ALERT_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@interface EditMenuAlertDelegateAction : NSObject
- (instancetype)initWithTitle:(NSString*)title
                       action:(ProceduralBlock)action
                        style:(UIAlertActionStyle)style
                    preferred:(BOOL)preferred NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The title of the action.
@property(nonatomic, copy, readonly) NSString* title;

// The action triggered.
@property(nonatomic, strong, readonly) ProceduralBlock action;

// The style of the button.
@property(nonatomic, assign, readonly) UIAlertActionStyle style;

// Whether this is the preferred action.
@property(nonatomic, assign, readonly) BOOL preferred;
@end

// Protocol to provide alert presenter to edit menu handlers.
@protocol EditMenuAlertDelegate

// Present the alert with the given title, message and actions.
- (void)showAlertWithTitle:(NSString*)title
                   message:(NSString*)message
                   actions:(NSArray<EditMenuAlertDelegateAction*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_EDIT_MENU_ALERT_DELEGATE_H_
