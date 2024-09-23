// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_ALERT_ACTION_H_
#define IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_ALERT_ACTION_H_

#import <UIKit/UIKit.h>

// This class is a replacement for UIAlertAction. UIAlertAction doesn't expose
// its handler, rendering it unreusable.
@interface AlertAction : NSObject

// The title for this action.
@property(nonatomic, readonly) NSString* title;

// The style for this action. Matches UIAlertAction style.
@property(nonatomic, readonly) UIAlertActionStyle style;

// The unique identifier for the actions created.
@property(nonatomic, readonly) NSInteger uniqueIdentifier;

// Block to be called when this action is triggered.
@property(nonatomic, readonly) void (^handler)(AlertAction* action);

// Initializes an action with `title` and `handler`.
+ (instancetype)actionWithTitle:(NSString*)title
                          style:(UIAlertActionStyle)style
                        handler:(void (^)(AlertAction* action))handler;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_ALERT_ACTION_H_
