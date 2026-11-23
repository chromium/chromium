// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LIVE_ACTIVITY_MODEL_IOS_CHROME_LIVE_ACTIVITY_MANAGER_H_
#define IOS_CHROME_BROWSER_LIVE_ACTIVITY_MODEL_IOS_CHROME_LIVE_ACTIVITY_MANAGER_H_

#import "components/keyed_service/core/keyed_service.h"

// Feature-agnostic profile-scoped `KeyedService` managing Live Activities.
class IOSChromeLiveActivityManager : public KeyedService {
 public:
  IOSChromeLiveActivityManager();
  IOSChromeLiveActivityManager(const IOSChromeLiveActivityManager&) = delete;
  IOSChromeLiveActivityManager& operator=(const IOSChromeLiveActivityManager&) =
      delete;
  ~IOSChromeLiveActivityManager() override;
};

#endif  // IOS_CHROME_BROWSER_LIVE_ACTIVITY_MODEL_IOS_CHROME_LIVE_ACTIVITY_MANAGER_H_
