// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_URL_CONTEXT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_URL_CONTEXT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GaiaId;

// Account switching types.
enum class AccountSwitchType {
  kSignIn,
  kSignOut,
};

// Context information for an URL with a request to switch account.
@interface URLContext : NSObject
- (instancetype)initWithContext:(UIOpenURLContext*)context
                         gaiaID:(const GaiaId&)gaiaID
                           type:(AccountSwitchType)type;

@property(nonatomic, readonly) UIOpenURLContext* context;
@property(nonatomic, readonly) GaiaId gaiaID;
@property(nonatomic, readonly) AccountSwitchType type;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_URL_CONTEXT_H_
