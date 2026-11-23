// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_MODEL_FIRST_RUN_LIVE_ACTIVITY_SERVICE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_MODEL_FIRST_RUN_LIVE_ACTIVITY_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"

class IOSChromeLiveActivityManager;

// Profile-scoped `KeyedService` coordinating the First Run Experience (FRE)
// Onboarding Live Activity on top of `IOSChromeLiveActivityManager`.
class FirstRunLiveActivityService : public KeyedService {
 public:
  explicit FirstRunLiveActivityService(
      IOSChromeLiveActivityManager* live_activity_manager);
  FirstRunLiveActivityService(const FirstRunLiveActivityService&) = delete;
  FirstRunLiveActivityService& operator=(const FirstRunLiveActivityService&) =
      delete;
  ~FirstRunLiveActivityService() override;

  // `KeyedService` implementation.
  void Shutdown() override;

 private:
  // Non-owning pointer to the profile-scoped `IOSChromeLiveActivityManager`,
  // owned by `IOSChromeLiveActivityManagerFactory`. Guaranteed non-null until
  // cleared in `Shutdown()`.
  raw_ptr<IOSChromeLiveActivityManager> live_activity_manager_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_LIVE_ACTIVITY_MODEL_FIRST_RUN_LIVE_ACTIVITY_SERVICE_H_
