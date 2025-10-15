// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_IDENTITY_CHOOSER_IDENTITY_CHOOSER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_IDENTITY_CHOOSER_IDENTITY_CHOOSER_MEDIATOR_H_

#import <UIKit/UIKit.h>

class ChromeAccountManagerService;
class GaiaId;
@protocol IdentityChooserConsumer;
@protocol SystemIdentity;
namespace signin {
class IdentityManager;
}  // namespace signin

// A mediator object that monitors updates of chrome identities, and updates the
// IdentityChooserViewController.
@interface IdentityChooserMediator : NSObject

// The designated initializer.
- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Selected Chrome identity.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;
// View controller.
@property(nonatomic, weak) id<IdentityChooserConsumer> consumer;

// Starts this mediator.
- (void)start;

// Disconnect the mediator.
- (void)disconnect;

// Selects an identity with a Gaia ID.
- (void)selectIdentityWithGaiaID:(const GaiaId&)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_IDENTITY_CHOOSER_IDENTITY_CHOOSER_MEDIATOR_H_
