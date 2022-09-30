// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class TangibleSyncMediator;

// Delegate for the Sync mediator.
@protocol TangibleSyncMediatorDelegate

// Notifies the delegate that `mediator` has finished sign in with success.
- (void)tangibleSyncMediatorDidSuccessfulyFinishSignin:
    (TangibleSyncMediator*)mediator;

// Notifies the delegate that the user has been removed.
- (void)tangibleSyncMediatorUserRemoved:(TangibleSyncMediator*)mediator;

// Sets the UI as interactable or not.
- (void)tangibleSyncMediator:(TangibleSyncMediator*)mediator
                   UIEnabled:(BOOL)UIEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_MEDIATOR_DELEGATE_H_
