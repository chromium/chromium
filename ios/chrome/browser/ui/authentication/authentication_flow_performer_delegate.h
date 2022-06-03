// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#include "ios/chrome/browser/signin/constants.h"

@class UIViewController;

// Handles completion of AuthenticationFlowPerformer steps.
@protocol AuthenticationFlowPerformerDelegate<NSObject>

// Indicates that a browser state was signed out.
- (void)didSignOut;

// Indicates that the user chose the clear data policy.
- (void)didChooseClearDataPolicy:(ShouldClearData)shouldClearData;

// Indicates that the user chose to cancel the operation.
- (void)didChooseCancel;

// Indicates that browsing data finished clearing.
- (void)didClearData;

// Indicates that the identity managed status was fetched.
- (void)didFetchManagedStatus:(NSString*)hostedDomain;

// Indicates that the requested identity managed status fetch failed.
- (void)didFailFetchManagedStatus:(NSError*)error;

// Indicates that the user accepted signing in to a managed account.
- (void)didAcceptManagedConfirmation;

// Indicates that the user cancelled signing in to a managed account.
- (void)didCancelManagedConfirmation;

// Dismisses the view controller that is showing the sign-in flow.
- (void)dismissPresentingViewControllerAnimated:(BOOL)animated
                                     completion:(ProceduralBlock)completion;

// Presents a view controller on the sign-in flow.
- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_DELEGATE_H_
