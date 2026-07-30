// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/metrics/model/activity_reporter.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// A minimal subclass of SigninCoordinator because it is abstract.
@interface TestSigninCoordinator : SigninCoordinator
@end

@implementation TestSigninCoordinator

- (BOOL)viewWillPersist {
  return NO;
}

@end

class SigninCoordinatorTest : public PlatformTest {
 public:
  SigninCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();

    scene_state_ = [[SceneState alloc] init];
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state_;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  void SetUp() override {
    PlatformTest::SetUp();

    coordinator_ = [[TestSigninCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                      contextStyle:SigninContextStyle::kDefault
                       accessPoint:signin_metrics::AccessPoint::kSettings];

    // signinCompletion must be set before start.
    coordinator_.signinCompletion =
        ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
          id<SystemIdentity> identity) {
        };
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  ProfileState* profile_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  TestSigninCoordinator* coordinator_ = nil;
};

TEST_F(SigninCoordinatorTest, ActivityReporting) {
  id mockInstance = OCMClassMock([ActivityReporter class]);
  [coordinator_ setValue:mockInstance forKey:@"activityReporter"];

  OCMExpect([mockInstance reportActive]);
  [coordinator_ start];
  [mockInstance verify];

  OCMExpect([mockInstance reportInactive]);
  [coordinator_ stopAnimated:NO];
  [mockInstance verify];

  [coordinator_ setValue:nil forKey:@"activityReporter"];
  [mockInstance stopMocking];
}
