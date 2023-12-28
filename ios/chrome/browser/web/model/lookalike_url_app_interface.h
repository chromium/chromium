// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_LOOKALIKE_URL_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_LOOKALIKE_URL_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for lookalike URL blocking page tests. It sets
// up LookalikeUrlDecider, which does the following:
//   - Lets a navigation proceed if the domain is explicitly allowed
//   - Cancels the navigation and shows error page with a suggested URL
//      for the /lookalike.html path
//   - Cancels the navigation and shows error page with no suggested URL
//      for the /lookalike-empty.html path
//   - Allows other navigations to proceed
@interface LookalikeUrlAppInterface : NSObject

// Sets up lookalike policy decider. Used for testing.
+ (void)setUpLookalikeUrlDeciderForWebState;

// Tear down lookalike policy decider. Used for testing.
+ (void)tearDownLookalikeUrlDeciderForWebState;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_LOOKALIKE_URL_APP_INTERFACE_H_
