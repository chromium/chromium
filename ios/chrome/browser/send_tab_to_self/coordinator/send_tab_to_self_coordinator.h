// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SigninPresenter;

class GURL;
namespace send_tab_to_self {
enum class ShareEntryPoint;
}
@protocol SendTabToSelfCoordinatorDelegate;

// Displays the send tab to self UI for all device form factors. Will show a
// modal dialog popup on both platforms. Once this coordinator is stopped, the
// underlying dialog is dismissed.
@interface SendTabToSelfCoordinator : ChromeCoordinator

// The delegate of this coordinator.
@property(nonatomic, weak) id<SendTabToSelfCoordinatorDelegate> delegate;

// Designated initializer. If `targetDeviceCacheGUID` is provided,
// the coordinator initializes in direct-send mode, sending the tab directly to
// that target device upon start (bypassing the picker UI).
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           signinPresenter:(id<SigninPresenter>)signinPresenter
                                       url:(const GURL&)url
                                     title:(NSString*)title
                     targetDeviceCacheGUID:(NSString*)targetDeviceCacheGUID
                          targetDeviceName:(NSString*)targetDeviceName
                                entryPoint:(send_tab_to_self::ShareEntryPoint)
                                               entryPoint
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer. Initializes the coordinator in the standard picker
// mode, presenting the list of target devices to the user to choose from.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           signinPresenter:(id<SigninPresenter>)signinPresenter
                                       url:(const GURL&)url
                                     title:(NSString*)title
                                entryPoint:(send_tab_to_self::ShareEntryPoint)
                                               entryPoint;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_COORDINATOR_H_
