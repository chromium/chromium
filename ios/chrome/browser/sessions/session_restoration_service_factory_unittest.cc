// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_restoration_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#include "testing/platform_test.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#include "ios/web/common/features.h"

class SessionRestorationServiceFactoryTest : public PlatformTest {
 public:
  SessionRestorationServiceFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kEnableSessionSerializationOptimizations);

    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  ChromeBrowserState* browser_state() { return browser_state_.get(); }

  ChromeBrowserState* otr_browser_state() {
    return browser_state_->GetOffTheRecordChromeBrowserState();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that the factory correctly instantiate a new service.
TEST_F(SessionRestorationServiceFactoryTest, CreateInstance) {
  EXPECT_TRUE(
      SessionRestorationServiceFactory::GetForBrowserState(browser_state()));
}

// Tests that the factory correctly instantiate a new service for off-the-record
// BrowserState.
TEST_F(SessionRestorationServiceFactoryTest, CreateOffTheRecordInstance) {
  EXPECT_TRUE(SessionRestorationServiceFactory::GetForBrowserState(
      otr_browser_state()));
}

// Tests that regular and off-the-record BrowserState uses distinct instances.
TEST_F(SessionRestorationServiceFactoryTest, InstancesAreDistinct) {
  EXPECT_NE(
      SessionRestorationServiceFactory::GetForBrowserState(browser_state()),
      SessionRestorationServiceFactory::GetForBrowserState(
          otr_browser_state()));
}
