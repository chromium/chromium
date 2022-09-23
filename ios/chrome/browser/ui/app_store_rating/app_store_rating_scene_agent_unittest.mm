// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing AppStoreRatingSceneAgent class.
class AppStoreRatingSceneAgentTest : public PlatformTest {
 protected:
  AppStoreRatingSceneAgentTest() {
    test_scene_agent_ = [[AppStoreRatingSceneAgent alloc] init];
  }

  ~AppStoreRatingSceneAgentTest() override {}

  AppStoreRatingSceneAgent* test_scene_agent_;
};
