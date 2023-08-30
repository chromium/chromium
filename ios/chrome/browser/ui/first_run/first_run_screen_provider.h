// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/screen/screen_provider.h"

class ChromeBrowserState;

// The class that provides a list of first run screens.
@interface FirstRunScreenProvider : ScreenProvider

- (instancetype)initForBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_PROVIDER_H_
