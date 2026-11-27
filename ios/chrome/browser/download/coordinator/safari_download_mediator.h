// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_SAFARI_DOWNLOAD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_SAFARI_DOWNLOAD_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"

@protocol SafariDownloadMediatorDelegate;
@protocol SafariDownloadTabHelperDelegate;

namespace web {
class WebState;
}  // namespace web

// Installs `SafariDownloadTabHelper` delegates on tabs and observes the active
// `WebState` to notify its delegate when stale download alerts should be
// dismissed.
@interface SafariDownloadMediator : NSObject <TabsDependencyInstalling>

// Delegate notified when active `WebState` lifecycle changes invalidate a
// presented download alert.
@property(nonatomic, weak) id<SafariDownloadMediatorDelegate> delegate;

// Delegate installed on each tab's `SafariDownloadTabHelper`. Must be set
// before observing the `WebStateList` via `TabsDependencyInstallerBridge`.
@property(nonatomic, weak) id<SafariDownloadTabHelperDelegate>
    tabHelperDelegate;

// Initializes the mediator to observe `activeWebState`, which may be null if no
// tab is currently active.
- (instancetype)initWithActiveWebState:(web::WebState*)activeWebState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops observing the active `WebState` and clears delegate references. Must be
// called before deallocation.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_SAFARI_DOWNLOAD_MEDIATOR_H_
