// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_CONTENT_UI_BUNDLED_TEST_FAKE_MAIN_CONTENT_UI_H_
#define IOS_CHROME_BROWSER_MAIN_CONTENT_UI_BUNDLED_TEST_FAKE_MAIN_CONTENT_UI_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/main_content/ui_bundled/main_content_ui.h"
#import "ios/chrome/browser/main_content/ui_bundled/main_content_ui_state.h"
#import "ios/chrome/browser/main_content/ui_bundled/test/test_main_content_ui_state.h"

// Fake implementation of `MainContentUI` for tests.
@interface FakeMainContentUI : NSObject <MainContentUI>

// Mutable test state returned as `MainContentUI`'s `mainContentUIState`.
@property(nonatomic, readonly) TestMainContentUIState* mainContentUIState;

// Updater initialized with `mainContentUIState` to simulate scroll and drag
// events in tests.
@property(nonatomic, strong, readonly) MainContentUIStateUpdater* stateUpdater;

@end

#endif  // IOS_CHROME_BROWSER_MAIN_CONTENT_UI_BUNDLED_TEST_FAKE_MAIN_CONTENT_UI_H_
