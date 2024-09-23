// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_TEST_REFRESHER_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_TEST_REFRESHER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

@class AppState;

// App refresher for collecting test data; not intended for production use.
@interface TestRefresher : AppRefreshProvider

// Don't use this as a precedent; an app state shouldn't be injected into a
// refresh provider.
- (instancetype)initWithAppState:(AppState*)appState;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_TEST_REFRESHER_H_
