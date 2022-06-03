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
  bool HasValidTargetDevice() override { return has_valid_target_device_; }

  void SetHasValidTargetDevice() { has_valid_target_device_ = true; }

 private:
  bool has_valid_target_device_ = false;
};

// TODO (crbug/974040): Move TestSendTabToSelfSyncService to components and
// reuse in both ios/chrome and chrome tests
class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  explicit TestSendTabToSelfSyncService(
      SendTabToSelfModel* send_tab_to_self_model)
      : send_tab_to_self_model_(send_tab_to_self_model) {}
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return send_tab_to_self_model_;
  }

 protected:
  SendTabToSelfModel* const send_tab_to_self_model_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    SendTabToSelfModel* send_tab_to_self_model,
    web::BrowserState* context) {
  return std::make_unique<TestSendTabToSelfSyncService>(send_tab_to_self_model);
}

class SendTabToSelfUtilTest : public PlatformTest {
 public:
  SendTabToSelfUtilTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }
  ~SendTabToSelfUtilTest() override {}

  ChromeBrowserState* browser_state() { return browser_state_.get(); }
  ChromeBrowserState* OffTheRecordChromeBrowserState() {
    return browser_state_->GetOffTheRecordChromeBrowserState();
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

TEST_F(SendTabToSelfUtilTest, HasValidTargetDevice) {
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      browser_state(), base::BindRepeating(&BuildTestSendTabToSelfSyncService,
                                           &send_tab_to_self_model_mock_));

  EXPECT_FALSE(HasValidTargetDevice(browser_state()));

  send_tab_to_self_model_mock_.SetHasValidTargetDevice();
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
