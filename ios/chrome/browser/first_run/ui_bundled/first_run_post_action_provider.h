// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_POST_ACTION_PROVIDER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_POST_ACTION_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"

/// Provider for first run actions that should be performed sequentially after
/// the FRE screens are dismissed.
@interface FirstRunPostActionProvider : ScreenProvider

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_POST_ACTION_PROVIDER_H_
