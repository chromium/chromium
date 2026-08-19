// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_URL_CONTEXT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_URL_CONTEXT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GaiaId;

// Account switching types triggered by an incoming URL context.
enum class AccountSwitchType {
  // Sign in to the account specified by the Gaia ID.
  kSignIn,
  // Sign out of the current account.
  kSignOut,
};

// Context information for an URL with a request to switch account.
@interface URLContext : NSObject

// Initializes a `URLContext` with the underlying iOS `UIOpenURLContext`, the
// target `gaiaID`, and the requested `type` of account switch.
- (instancetype)initWithContext:(UIOpenURLContext*)context
                         gaiaID:(const GaiaId&)gaiaID
                           type:(AccountSwitchType)type;

// The underlying iOS URL context containing the URL to be opened.
@property(nonatomic, readonly) UIOpenURLContext* context;

// The Gaia ID to switch to, or empty / `kNoAccount` for sign-out.
@property(nonatomic, readonly) GaiaId gaiaID;

// The account switch action to perform (sign in or sign out).
@property(nonatomic, readonly) AccountSwitchType type;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_URL_CONTEXT_H_
