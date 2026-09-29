// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for `TTCKeyedService` and `TTCKeyedServiceFactory`.
class TTCKeyedServiceTest : public PlatformTest {
 public:
  TTCKeyedServiceTest() = default;
  ~TTCKeyedServiceTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    InitProfile(/*enable_ttc=*/true);
  }

  void TearDown() override {
    profile_.reset();
    PlatformTest::TearDown();
  }

  void InitProfile(bool enable_ttc) {
    profile_.reset();
    if (enable_ttc) {
      scoped_feature_list_.Reset();
      scoped_feature_list_.InitAndEnableFeature(kTTCEnabled);
      TTCKeyedServiceFactory::GetInstance();
    } else {
      scoped_feature_list_.Reset();
      scoped_feature_list_.InitAndDisableFeature(kTTCEnabled);
    }
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Test that `TTCKeyedService` is instantiated for regular profile when enabled.
TEST_F(TTCKeyedServiceTest, TestServiceCreatedForRegularProfileWhenEnabled) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);
  EXPECT_TRUE(service->IsEnabled());
  EXPECT_EQ(service->profile(), profile_.get());
}

// Test that `TTCKeyedService` returns nullptr when feature flag is disabled.
TEST_F(TTCKeyedServiceTest, TestServiceNotCreatedWhenFeatureDisabled) {
  InitProfile(/*enable_ttc=*/false);
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  EXPECT_EQ(service, nullptr);
}

// Test that `TTCKeyedService` is not created for off-the-record profiles.
TEST_F(TTCKeyedServiceTest, TestServiceNotCreatedForIncognitoProfile) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories();
  ASSERT_TRUE(otr_profile != nullptr);
  TTCKeyedService* service = TTCKeyedService::Get(otr_profile);
  EXPECT_EQ(service, nullptr);
}

// Test that initial service state is inactive and controller is null.
TEST_F(TTCKeyedServiceTest, TestInitialStateIsSessionInactive) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);
  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->session_controller(), nil);
  EXPECT_EQ(service->GetSessionLifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(service->GetState(), TTCServiceState::kSessionInactive);
}

// Test that starting and ending a session transitions service state and
// lifecycle.
TEST_F(TTCKeyedServiceTest, TestStartAndEndSessionTransitionsState) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);

  service->StartSession();
  EXPECT_TRUE(service->is_session_active());
  ASSERT_TRUE(service->session_controller() != nil);
  EXPECT_EQ(service->GetSessionLifecycle(), TTCSessionLifecycle::kInitializing);
  EXPECT_EQ(service->GetState(), TTCServiceState::kSessionActive);

  service->EndSession();
  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->session_controller(), nil);
  EXPECT_EQ(service->GetSessionLifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(service->GetState(), TTCServiceState::kSessionInactive);
}

// Test that ending a session when inactive is a safe no-op.
TEST_F(TTCKeyedServiceTest, TestEndSessionWhenInactiveIsNoOp) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);
  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->session_controller(), nil);

  service->EndSession();
  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->session_controller(), nil);
}

// Test that state change callbacks are invoked on start and end.
TEST_F(TTCKeyedServiceTest, TestStateChangedCallbackNotification) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);

  std::vector<TTCServiceState> received_states;
  base::CallbackListSubscription subscription =
      service->RegisterStateChangedCallback(base::BindRepeating(
          [](std::vector<TTCServiceState>* states, TTCServiceState state) {
            states->push_back(state);
          },
          base::Unretained(&received_states)));

  service->StartSession();
  service->EndSession();

  ASSERT_EQ(received_states.size(), 2u);
  EXPECT_EQ(received_states[0], TTCServiceState::kSessionActive);
  EXPECT_EQ(received_states[1], TTCServiceState::kSessionInactive);
}

// Test that Shutdown cleans up active sessions.
TEST_F(TTCKeyedServiceTest, TestShutdownEndsActiveSession) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);

  service->StartSession();
  EXPECT_TRUE(service->is_session_active());

  service->Shutdown();
  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->GetState(), TTCServiceState::kSessionInactive);
}

// Test that controller disconnect ends the session in TTCKeyedService.
TEST_F(TTCKeyedServiceTest, TestControllerDisconnectEndsSessionInService) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_NE(service, nullptr);

  service->StartSession();
  EXPECT_TRUE(service->is_session_active());
  TTCSessionController* controller = service->session_controller();
  ASSERT_NE(controller, nil);

  [controller disconnect];
  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->GetState(), TTCServiceState::kSessionInactive);
  EXPECT_EQ(service->session_controller(), nil);
}

// Test that background notification ends the session in TTCKeyedService.
TEST_F(TTCKeyedServiceTest, TestBackgroundNotificationEndsSessionInService) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_NE(service, nullptr);

  service->StartSession();
  EXPECT_TRUE(service->is_session_active());
  EXPECT_NE(service->session_controller(), nil);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];

  EXPECT_FALSE(service->is_session_active());
  EXPECT_EQ(service->GetState(), TTCServiceState::kSessionInactive);
  EXPECT_EQ(service->session_controller(), nil);
}

// Test that starting an already active session asserts/crashes.
TEST_F(TTCKeyedServiceTest, TestStartSessionCrashesIfAlreadyStarted) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);

  service->StartSession();
  EXPECT_DEATH_IF_SUPPORTED(service->StartSession(), "");
}

// Test that GetWeakPtr returns a valid weak pointer to the service.
TEST_F(TTCKeyedServiceTest, TestWeakPtrValidity) {
  TTCKeyedService* service = TTCKeyedService::Get(profile_.get());
  ASSERT_TRUE(service != nullptr);

  base::WeakPtr<TTCKeyedService> weak_ptr = service->GetWeakPtr();
  EXPECT_EQ(weak_ptr.get(), service);
}
