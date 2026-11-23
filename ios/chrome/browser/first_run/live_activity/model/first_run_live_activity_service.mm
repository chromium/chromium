// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/live_activity/model/first_run_live_activity_service.h"

#import "base/check.h"
#import "ios/chrome/browser/first_run/live_activity/public/features.h"

FirstRunLiveActivityService::FirstRunLiveActivityService(
    IOSChromeLiveActivityManager* live_activity_manager)
    : live_activity_manager_(live_activity_manager) {
  CHECK(IsFreLiveActivityEnabled());
  CHECK(live_activity_manager_);
  // TODO(crbug.com/466213758): Observe live_activity_manager_ and coordinate
  // Live Activity lifecycle.
}

FirstRunLiveActivityService::~FirstRunLiveActivityService() = default;

void FirstRunLiveActivityService::Shutdown() {
  live_activity_manager_ = nullptr;
}
