// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ALERT_COORDINATOR_ALERT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_ALERT_COORDINATOR_ALERT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// A coordinator specialization for the case where the coordinator is creating
// and managing a modal alert to be displayed to the user.
// Calling `-stop` on this coordinator dismisses the current alert with no
// animation then destroys it.
@interface AlertCoordinator : ChromeCoordinator

// Whether a cancel button has been added.
@property(nonatomic, readonly) BOOL cancelButtonAdded;
// Title of the alert.
@property(nonatomic, copy, readonly) NSString* title;
// Message of the alert.
@property(nonatomic, copy) NSString* message;
// Whether the alert is visible. This will be true after `-start` is called
// until a subsequent `-stop`.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;
// Handler executed when calling `-executeCancelHandler`. This handler is
// deleted when the alert is dismissed (user interaction or `-stop`).
@property(nonatomic, copy) ProceduralBlock cancelAction;
// Block called when the alert is about to be displayed.
@property(nonatomic, copy) ProceduralBlock startAction;
// Block called when the alert is stopped with `stop` or during dealloc. It is
// called only if no interaction with the alert (user interaction or call to
// `-executeCancelHandler`) has occurred.
@property(nonatomic, copy) ProceduralBlock noInteractionAction;

// Init a coordinator for displaying a alert on this view controller.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                   message:(NSString*)message
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Adds an item at the end of the menu. It does nothing if `visible` is true or
// if trying to add an item with a UIAlertActionStyleCancel while
// `cancelButtonAdded` is true. If `enabled` is NO, the action appears dimmed
// and non-interactable. If `preferred` is YES, the action will be in bold
// letters. Only one item can be preferred.
- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style
               preferred:(BOOL)preferred
                 enabled:(BOOL)enabled;
// Shorthand for the above method, with `preferred` = NO.
- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style
                 enabled:(BOOL)enabled;
// Shorthand for the above method, with `enabled` = YES.
- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style;

// Executes `cancelAction`.
- (void)executeCancelHandler;

@end

@interface AlertCoordinator (Subclassing)
// The UIAlertController being managed by this coordinator.
@property(nonatomic, readonly) UIAlertController* alertController;
// Called when lazily instantiating `alertController`.  Subclasses should
// override and return the appropriately configured UIAlertController.
- (UIAlertController*)alertControllerWithTitle:(NSString*)title
                                       message:(NSString*)message;
@end

#endif  // IOS_CHROME_BROWSER_UI_ALERT_COORDINATOR_ALERT_COORDINATOR_H_
