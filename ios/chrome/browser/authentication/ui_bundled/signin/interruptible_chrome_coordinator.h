// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INTERRUPTIBLE_CHROME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INTERRUPTIBLE_CHROME_COORDINATOR_H_

#import <Foundation/Foundation.h>

// Interface for a ChromeCoordinator that can be interrupted without following
// the conventional flow.
// TODO(crbug.com/381444097): Delete this protocol
@protocol InterruptibleChromeCoordinator <NSObject>

// Interrupt the coordinator to immediately tear down the views it manages.
// Depending on `animated`, the interruption may be done with/without an
// animation, or without dismissing the views when the interruption is for
// shutdown (e.g., tearing down the scene).
- (void)interruptAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INTERRUPTIBLE_CHROME_COORDINATOR_H_
