// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

// TODO (crbug/974040): update TestSendTabToSelfModel and delete this class
class SendTabToSelfModelMock : public TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  bool IsReady() override { return true; }
  bool HasValidTargetDevice() override { return true; }
};

// TODO (crbug/974040): Move TestSendTabToSelfSyncService to components and
// reuse in both ios/chrome and chrome tests
class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() = default;
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &send_tab_to_self_model_mock_;
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    web::BrowserState* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class SendTabToSelfUtilTest : public PlatformTest {
 public:
  SendTabToSelfUtilTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }
  ~SendTabToSelfUtilTest() override {}

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }
  ios::ChromeBrowserState* OffTheRecordChromeBrowserState() {
    return browser_state_->GetOffTheRecordChromeBrowserState();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
};

TEST_F(SendTabToSelfUtilTest, HasValidTargetDevice) {
  EXPECT_FALSE(HasValidTargetDevice(browser_state()));

  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      browser_state(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

  EXPECT_TRUE(HasValidTargetDevice(browser_state()));
}

TEST_F(SendTabToSelfUtilTest, NotHTTPOrHTTPS) {
  GURL url = GURL("192.168.0.0");
  EXPECT_FALSE(AreContentRequirementsMet(url, browser_state()));
}

TEST_F(SendTabToSelfUtilTest, WebUIPage) {
  GURL url = GURL("chrome://flags");
  EXPECT_FALSE(AreContentRequirementsMet(url, browser_state()));
}

TEST_F(SendTabToSelfUtilTest, IncognitoMode) {
  GURL url = GURL("https://www.google.com");
  EXPECT_FALSE(
      AreContentRequirementsMet(url, OffTheRecordChromeBrowserState()));
}

TEST_F(SendTabToSelfUtilTest, ValidUrl) {
  GURL url = GURL("https://www.google.com");
  EXPECT_TRUE(AreContentRequirementsMet(url, browser_state()));
}

// TODO(crbug.com/961897) Add test for CreateNewEntry.

}  // namespace send_tab_to_self
