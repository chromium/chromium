// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACKGROUND_SYNC_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_BACKGROUND_SYNC_TEST_UTIL_H_

namespace content {
class WebContents;

// Utility namespace for background sync tests.
namespace background_sync_test_util {

// Enables or disables notifications coming from the NetworkConnectionTracker.
// (For preventing flakes in tests)
void SetIgnoreNetworkChanges(bool ignore);

// Puts background sync manager into online or offline mode for tests.
//
// Must be called on the UI thread.
void SetOnline(WebContents* web_contents, bool online);

}  // namespace background_sync_test_util

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACKGROUND_SYNC_TEST_UTIL_H_
