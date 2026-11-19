// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_PROVIDER_PROTECTED_H_
#define IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_PROVIDER_PROTECTED_H_

#import <vector>

#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"

// Method available only for subclasses of ScreenProvider.
@interface ScreenProvider ()

// Initiate the provider with the screens to display.
- (instancetype)initWithScreens:(std::vector<ScreenType>)screens;

@end

#endif  // IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_PROVIDER_PROTECTED_H_
