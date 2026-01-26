// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class IdentityChooserCoordinator;
@protocol SystemIdentity;

// Delegate protocol for IdentityChooserCoordinator.
@protocol IdentityChooserCoordinatorDelegate <NSObject>

// Called when the user taps on "Add Account…" button. The view controller is
// already dismissed when this call is made.
- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator;

// Called when the user selects an identity. The view controller is
// already dismissed when this call is made. The identity may be nil in case of
// race condition.
- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
      didCloseWithSelectedIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_DELEGATE_H_
