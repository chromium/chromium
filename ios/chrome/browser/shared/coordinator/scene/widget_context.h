// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_WIDGET_CONTEXT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_WIDGET_CONTEXT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Account switching types.
enum class AccountSwitchType {
  kSignIn,
  kSignOut,
};

// Context information for an URL coming from widgets.
@interface WidgetContext : NSObject
- (instancetype)initWithContext:(UIOpenURLContext*)context
                         gaiaID:(NSString*)gaiaID
                           type:(AccountSwitchType)type;

@property(nonatomic, readonly) UIOpenURLContext* context;
@property(nonatomic, readonly) NSString* gaiaID;
@property(nonatomic, readonly) AccountSwitchType type;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_WIDGET_CONTEXT_H_
