// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_TEST_SYNC_SERVICE_UTILS_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_TEST_SYNC_SERVICE_UTILS_H_

#include <memory>

class KeyedService;
class ProfileIOS;

// Returns a TestSyncService.
std::unique_ptr<KeyedService> CreateTestSyncService(ProfileIOS& profile);

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_TEST_SYNC_SERVICE_UTILS_H_
