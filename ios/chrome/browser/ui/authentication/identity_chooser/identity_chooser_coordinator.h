// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol IdentityChooserCoordinatorDelegate;
@protocol SystemIdentity;

// Coordinator to display the identity chooser view controller.
@interface IdentityChooserCoordinator : ChromeCoordinator

// Selected identity.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;
// Delegate.
@property(nonatomic, weak) id<IdentityChooserCoordinatorDelegate> delegate;
// Origin of the animation for the IdentityChooser.
@property(nonatomic, assign) CGPoint origin;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_H_
