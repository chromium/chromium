// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager.h"

#import "base/check.h"
#import "ios/chrome/browser/live_activity/public/features.h"

IOSChromeLiveActivityManager::IOSChromeLiveActivityManager() {
  CHECK(IsIOSLiveActivityManagerEnabled());
  // TODO(crbug.com/466213758): Initialize LiveActivityHandler and observer
  // bridge.
}

IOSChromeLiveActivityManager::~IOSChromeLiveActivityManager() = default;
