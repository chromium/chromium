// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_COORDINATOR_H_

#import <optional>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class RecipientInfoForIOSDisplay;
@protocol SharingStatusCoordinatorDelegate;

class GURL;

// This coordinator presents a view with a sharing status animation. Main part
// of the animation is a progress bar loading between images of the sender and
// the recipients. The progress does not reflect actual sharing going on under
// the hood, but rather has a fixed time that allows the user to cancel sharing.
// If the user does not cancel it, then the API is called to process sharing and
// success status is displayed. Otherwise, cancelled status is displayed.
@interface SharingStatusCoordinator : ChromeCoordinator

// `recipients` contains information about users selected to receive passwords.
// `website` is a display origin of the site for which the password is shared.
// `URL` is a url of the website for which the password is being shared.
// `changePasswordURL` is a url which allows to change the password that is
// being shared (might be null for Android app passwords).
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
                       website:(NSString*)website
                           URL:(const GURL&)URL
             changePasswordURL:(const std::optional<GURL>&)changePasswordURL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<SharingStatusCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_COORDINATOR_H_
