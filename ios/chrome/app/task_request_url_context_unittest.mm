// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_url_context.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class TaskRequestForURLContextTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();

    profile_ = TestProfileIOS::Builder().Build();
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
  }

  void TearDown() override {
    browser_.reset();
    [scene_state_ shutdown];
    scene_state_ = nil;
    profile_state_ = nil;
    profile_.reset();
    ResetEnableNewStartupFlowEnabledForTesting();
    PlatformTest::TearDown();
  }

  UIOpenURLContext* CreateMockURLContext(NSURL* url) {
    id mockContext = OCMClassMock([UIOpenURLContext class]);
    OCMStub([mockContext URL]).andReturn(url);
    return mockContext;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that Startup.MobileSessionStartFromApps is logged.
TEST_F(TaskRequestForURLContextTest, TestStartupMobileSessionStartFromApps) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"https://www.example.com"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [[TaskRequestForURLContext alloc] initWithURLContext:context
                                                sceneState:scene_state_
                                               isColdStart:YES];
  EXPECT_NE(request, nil);

  histogram_tester.ExpectTotalCount("Startup.MobileSessionStartFromApps", 1);
}

// Tests that Startup.ShowDefaultPromoFromApps is logged when URL contains the
// default browser settings query item.
TEST_F(TaskRequestForURLContextTest, TestStartupShowDefaultPromoFromApps) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"https://www.example.com?poa=default-browser-settings"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [[TaskRequestForURLContext alloc] initWithURLContext:context
                                                sceneState:scene_state_
                                               isColdStart:YES];
  EXPECT_NE(request, nil);

  histogram_tester.ExpectTotalCount("Startup.ShowDefaultPromoFromApps", 1);
  histogram_tester.ExpectBucketCount("Startup.ShowDefaultPromoFromApps",
                                     CALLER_APP_THIRD_PARTY, 1);
}
