// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_SAFARI_DOWNLOAD_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_SAFARI_DOWNLOAD_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class SafariDownloadMediator;

namespace web {
class WebState;
}  // namespace web

// Delegate protocol for SafariDownloadMediator.
@protocol SafariDownloadMediatorDelegate <NSObject>

// Called when the active `webState` navigates cross-document, is hidden,
// deactivated, closed, detached, or replaced, and any download alert presented
// for it should be dismissed.
- (void)safariDownloadMediator:(SafariDownloadMediator*)mediator
    shouldDismissAlertForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_SAFARI_DOWNLOAD_MEDIATOR_DELEGATE_H_
