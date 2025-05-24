// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GOOGLE_ONE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GOOGLE_ONE_COMMANDS_H_

#import "ios/chrome/browser/google_one/shared/google_one_entry_point.h"

// Commands related to GoogleOne.
@protocol GoogleOneCommands

// Presents a Google One coordinator to present account management related to
// `identity` on `viewController`.
- (void)showGoogleOneForIdentity:(id<SystemIdentity>)identity
                      entryPoint:(GoogleOneEntryPoint)entryPoint
              baseViewController:(UIViewController*)baseViewController;

// Hides the Google One controller and stop the coordinator.
- (void)hideGoogleOne;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GOOGLE_ONE_COMMANDS_H_
