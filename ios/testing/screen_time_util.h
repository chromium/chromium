// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_SCREEN_TIME_UTIL_H_
#define IOS_TESTING_SCREEN_TIME_UTIL_H_

// On the simulator, waits for the `ScreenTimeAgent` and (on iOS 27+)
// `ScreenTimeSettingsAgent` daemons to finish post-boot initialization and
// respond to configuration requests. Otherwise, `WKWebView` frame commits
// trigger synchronous `ScreenTime` XPC calls on the Swift Concurrency
// cooperative thread pool that contend with `BrowserEngineKit` web content
// process launch on cold-booted simulators, and the first loads of a test suite
// can hang. Test suites that create web views should call this once, before
// running tests. Does nothing on devices.
void EnsureScreenTimeReady();

#endif  // IOS_TESTING_SCREEN_TIME_UTIL_H_
