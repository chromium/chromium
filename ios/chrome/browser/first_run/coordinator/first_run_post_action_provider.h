// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_COORDINATOR_FIRST_RUN_POST_ACTION_PROVIDER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_COORDINATOR_FIRST_RUN_POST_ACTION_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"

class PrefService;

/// Provider for first run actions that should be performed sequentially after
/// the FRE screens are dismissed.
@interface FirstRunPostActionProvider : ScreenProvider

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Alerts the provider that the guided tour was started.
- (void)setGuidedTourStarted:(BOOL)started;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_COORDINATOR_FIRST_RUN_POST_ACTION_PROVIDER_H_
