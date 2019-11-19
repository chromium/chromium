// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_PRESENTING_H_
#define IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_PRESENTING_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// A protocol for objects that present Sign In UI.
@protocol SigninInteractionPresenting

// Presents |viewController|. |animated| determines if the presentation is
// animated or not. |completion| is run after |viewController| is presented.
- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(ProceduralBlock)completion;

// Presents |viewController| at the top of the presentation hierarchy.
- (void)presentTopViewController:(UIViewController*)viewController
                        animated:(BOOL)animated
                      completion:(ProceduralBlock)completion;

// Dismisses all view controllers presented via this protocol.
- (void)dismissAllViewControllersAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion;

// Presents a dialog for |error| at the top of the presentation hierarchy.
// |dismissAction| is run when the dialog is dismissed. This method should not
// be called while an error is already being presented.
- (void)presentError:(NSError*)error
       dismissAction:(ProceduralBlock)dismissAction;

// Dismisses the error dialog.
- (void)dismissError;

// Indicates whether the object is currently presenting.
@property(nonatomic, assign, readonly, getter=isPresenting) BOOL presenting;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_PRESENTING_H_
