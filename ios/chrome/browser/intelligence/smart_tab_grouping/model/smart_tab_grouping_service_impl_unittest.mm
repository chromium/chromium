// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/smart_tab_grouping/model/smart_tab_grouping_service_impl.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/mock_callback.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/optimization_guide/model/fake_optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace ai {

namespace {

// Helper function to create a FakeOptimizationGuideService.
std::unique_ptr<KeyedService> CreateFakeOptimizationGuideService(
    ProfileIOS* profile) {
  return std::make_unique<FakeOptimizationGuideService>(
      profile->GetProtoDatabaseProvider(), profile->GetStatePath(),
      profile->IsOffTheRecord(), "en",
      base::WeakPtr<optimization_guide::OptimizationGuideStore>(),
      profile->GetPrefs(), nullptr, nullptr,
      IdentityManagerFactory::GetForProfile(profile));
}

class SmartTabGroupingServiceImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Set up the profile builder with fake OptimizationGuideService and
    // IdentityManager.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeOptimizationGuideService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    // Cache the fake service pointer for testing.
    fake_optimization_guide_service_ =
        static_cast<FakeOptimizationGuideService*>(
            OptimizationGuideServiceFactory::GetForProfile(profile_.get()));

    // Set up the WebStateList.
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    // Create an active web state to avoid crash in constructor.
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state_list_->InsertWebState(std::move(web_state));
    web_state_list_->ActivateWebStateAt(0);

    // Sign in a user to avoid potential issues or to test identity changes.
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        "test@example.com", signin::ConsentLevel::kSignin);

    // Initialize the service under test.
    service_ = std::make_unique<SmartTabGroupingServiceImpl>(
        remote_.BindNewPipeAndPassReceiver(), web_state_list_.get(), nullptr);
  }

  void TearDown() override {
    service_.reset();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  raw_ptr<FakeOptimizationGuideService> fake_optimization_guide_service_;
  std::unique_ptr<SmartTabGroupingServiceImpl> service_;
  mojo::Remote<ai::mojom::SmartTabGroupingService> remote_;
};

// Tests that a successful model execution returns a response.
TEST_F(SmartTabGroupingServiceImplTest, ExecuteRequestSuccess) {
  base::MockCallback<
      base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>>
      callback;

  // Set up fake response.
  optimization_guide::proto::IosSmartTabGroupingResponse response;
  fake_optimization_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kIosSmartTabGrouping,
      response, "optimization_guide.proto.IosSmartTabGroupingResponse");

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce(
          [&run_loop](ai::mojom::SmartTabGroupingResponseResultPtr result) {
            EXPECT_TRUE(result->is_response());
            run_loop.Quit();
          });

  service_->ExecuteSmartTabGroupingRequest(callback.Get());

  // Wait for wrapper to complete and call ExecuteModel.
  run_loop.Run();
}

// Tests that an error is returned if the WebStateList is empty.
TEST_F(SmartTabGroupingServiceImplTest, EmptyWebStateList) {
  base::MockCallback<
      base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>>
      callback;

  // Clear web state list.
  while (!web_state_list_->empty()) {
    web_state_list_->CloseWebStateAt(0, WebStateList::ClosingReason::kDefault);
  }

  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([](ai::mojom::SmartTabGroupingResponseResultPtr result) {
        EXPECT_TRUE(result->is_error());
        EXPECT_EQ(result->get_error(), "WebStateList empty");
      });

  service_->ExecuteSmartTabGroupingRequest(callback.Get());
}

// Tests that a pending request is cancelled when a new request is made.
TEST_F(SmartTabGroupingServiceImplTest, CancelOnNewRequest) {
  base::MockCallback<
      base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>>
      callback1;
  base::MockCallback<
      base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>>
      callback2;

  EXPECT_CALL(callback1, Run(testing::_))
      .WillOnce([](ai::mojom::SmartTabGroupingResponseResultPtr result) {
        EXPECT_TRUE(result->is_error());
        EXPECT_EQ(result->get_error(),
                  "Request superseded by a new smart tab grouping request.");
      });

  EXPECT_CALL(callback2, Run(testing::_))
      .WillOnce([](ai::mojom::SmartTabGroupingResponseResultPtr result) {
        EXPECT_TRUE(result->is_error());
        EXPECT_EQ(result->get_error(), "Service shutting down");
      });

  service_->ExecuteSmartTabGroupingRequest(callback1.Get());
  service_->ExecuteSmartTabGroupingRequest(callback2.Get());

  // Reset service to trigger cancellation callback while mocks are still in
  // scope.
  service_.reset();
}

// Tests that a pending request is cancelled when the user signs out.
TEST_F(SmartTabGroupingServiceImplTest, CancelOnIdentityChange) {
  base::MockCallback<
      base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>>
      callback;

  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([](ai::mojom::SmartTabGroupingResponseResultPtr result) {
        EXPECT_TRUE(result->is_error());
        EXPECT_EQ(result->get_error(), "Primary account was changed.");
      });

  service_->ExecuteSmartTabGroupingRequest(callback.Get());

  // Change identity.
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
}

// Tests that an error is returned when the model execution fails.
TEST_F(SmartTabGroupingServiceImplTest, ErrorOnModelExecutionFailure) {
  base::MockCallback<
      base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>>
      callback;

  // Set up fake error.
  fake_optimization_guide_service_->SetError(
      optimization_guide::ModelBasedCapabilityKey::kIosSmartTabGrouping,
      static_cast<int>(
          optimization_guide::OptimizationGuideModelExecutionError::
              ModelExecutionError::kGenericFailure));

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce(
          [&run_loop](ai::mojom::SmartTabGroupingResponseResultPtr result) {
            EXPECT_TRUE(result->is_error());
            EXPECT_NE(result->get_error().find("Server Model Execution Error:"),
                      std::string::npos);
            run_loop.Quit();
          });

  service_->ExecuteSmartTabGroupingRequest(callback.Get());

  // Wait for wrapper to complete and call ExecuteModel.
  run_loop.Run();
}

}  // namespace

}  // namespace ai
