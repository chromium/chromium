// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_MEDIATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

@class SyncScreenMediator;

// Delegate for the Sync mediator.
@protocol SyncScreenMediatorDelegate

// Notifies the delegate that `mediator` has finished sign in with success.
- (void)syncScreenMediatorDidSuccessfulyFinishSignin:
    (SyncScreenMediator*)mediator;

// Notifies the delegate that the user has been removed.
- (void)userRemoved;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_MEDIATOR_DELEGATE_H_
