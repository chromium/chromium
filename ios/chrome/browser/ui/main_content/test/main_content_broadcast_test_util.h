// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONTENT_TEST_MAIN_CONTENT_BROADCAST_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONTENT_TEST_MAIN_CONTENT_BROADCAST_TEST_UTIL_H_

@class ChromeBroadcaster;
@class TestMainContentUIState;

// Checks whether `ui_state`'s broadcast properties are being broadcast through
// `broadcaster`. Verifies broadcast setup according to `should_broadcast`.
void VerifyMainContentUIBroadcast(TestMainContentUIState* ui_state,
                                  ChromeBroadcaster* broadcaster,
                                  bool should_broadcast);

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONTENT_TEST_MAIN_CONTENT_BROADCAST_TEST_UTIL_H_
