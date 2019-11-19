// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACKGROUND_SYNC_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BACKGROUND_SYNC_TEST_UTILS_H_

namespace content {
class WebContents;

// Utility namespace for background sync tests.
namespace background_sync_test_util {

// Enables or disables notifications coming from the NetworkConnectionTracker.
// (For preventing flakes in tests)
void SetIgnoreNetworkChanges(bool ignore);

// Puts background sync manager into online or offline mode for tests.
//
// This eventually (asynchronously) runs on the service worker core thread.
// However you can start performing background sync operations without waiting
// for the core thread task to complete, since those background sync operations
// also run on the core thread.
void SetOnline(WebContents* web_contents, bool online);

}  // namespace background_sync_test_util

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACKGROUND_SYNC_TEST_UTILS_H_
