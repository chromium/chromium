// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_ROOT_DRIVE_FILE_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_ROOT_DRIVE_FILE_PICKER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SystemIdentity;
@protocol ComposeboxPickerPresenterDelegate;
namespace web {
class WebState;
}

// Coordinator of the Drive file picker.
@interface RootDriveFilePickerCoordinator : ChromeCoordinator

// Delegate to forward Composebox picker results to.
@property(nonatomic, weak) id<ComposeboxPickerPresenterDelegate>
    composeboxDelegate;

// Creates a coordinator that uses `viewController`, `browser` and `webState`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                             forComposebox:(BOOL)forComposebox
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Set the selected identity.
- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_ROOT_DRIVE_FILE_PICKER_COORDINATOR_H_
