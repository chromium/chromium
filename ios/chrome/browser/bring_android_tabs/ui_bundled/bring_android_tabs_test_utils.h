// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_TEST_UTILS_H_

#import <Foundation/Foundation.h>

struct AppLaunchConfiguration;
enum class BringAndroidTabsTestSession;
class GURL;

// Returns the app launch configuration and forces first run experience for
// Android switcher, if `is_android_switcher` is YES, or non Android switcher if
// NO.
AppLaunchConfiguration GetConfiguration(BOOL is_android_switcher);

// On first run experience promos, signs in, enables history/tab sync if
// `enable_history_sync` is YES, and dismisses the default browser promo to show
// the new tab page.
void SignInViaFREWithHistorySyncEnabled(BOOL enable_history_sync);

// Adds a session to fake sync server.
void AddSessionToFakeSyncServerFromTestServer(
    BringAndroidTabsTestSession session,
    const GURL& test_server);

// Verifies the visual state of the prompt. If `visibility` is YES, this
// verification passes that the prompt is visible; otherwise it passes when the
// prompt is invisible.
void VerifyConfirmationAlertPromptVisibility(BOOL visibility);
void VerifyTabListPromptVisibility(BOOL visibility);

// Returns the number of tabs from distant sessions that should be shown on the
// prompt.
int GetTabCountOnPrompt();

// Restarts the app, goes to the tab grid, and verifies that the prompt is not
// shown. Test server host should be passed to set up the premise that tabs from
// Android are available.
void VerifyThatPromptDoesNotShowOnRestart(const GURL& test_server);

// Factory resets the state of the app. Should be called at the end of each
// test.
void CleanUp();

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_TEST_UTILS_H_
