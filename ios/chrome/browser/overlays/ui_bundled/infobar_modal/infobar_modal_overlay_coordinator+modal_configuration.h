// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_COORDINATOR_MODAL_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_COORDINATOR_MODAL_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator.h"

@class InfobarModalOverlayMediator;

// Category implemented by InfobarModalOverlayCoordinator subclasses to
// configure the view to display for infobar modals.
@interface InfobarModalOverlayCoordinator (ModalConfiguration)

// The mediator used to configure the modal view controller.  Created in
// `-configureModal`.
@property(nonatomic, readonly) InfobarModalOverlayMediator* modalMediator;
// The view controller to display for the infobar modal.  Created in
// `-configureModal`.  This view controller is not the view controller returned
// by the InfobarModalOverlayCoordinator.viewController property, but is added
// as a child view controller to the top-level infobar modal container view.
@property(nonatomic, readonly) UIViewController* modalViewController;

// Creates a modal view controller and configures it with a new mediator.
// Resets `modalViewController` and `modalMediator` to the new instances.
- (void)configureModal;

// Resets `modalTransitionDriver` and `modalNavController`. Reassigns `mediator`
// to `modalMediator`.
- (void)configureViewController;

// Resets `modalMediator` and `modalViewController`.
- (void)resetModal;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_COORDINATOR_MODAL_CONFIGURATION_H_
