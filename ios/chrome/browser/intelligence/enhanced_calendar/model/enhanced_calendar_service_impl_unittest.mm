// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/mock_callback.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/fake_optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/mojom/enhanced_calendar_service.mojom.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace ai {

namespace {

class FakeOptimizationGuideService : public ::FakeOptimizationGuideService {
 public:
  using ::FakeOptimizationGuideService::FakeOptimizationGuideService;

  void ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      const optimization_guide::ModelExecutionOptions& options,
      optimization_guide::OptimizationGuideModelExecutionResultCallback
          callback) override {
    feature_ = feature;
    // Deep copy the request.
    last_request_.reset(request_metadata.New());
    last_request_->CheckTypeAndMergeFrom(request_metadata);

    ::FakeOptimizationGuideService::ExecuteModel(feature, request_metadata,
                                                 options, std::move(callback));
  }

  optimization_guide::ModelBasedCapabilityKey feature_;
  std::unique_ptr<google::protobuf::MessageLite> last_request_;
};

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

}  // namespace

class EnhancedCalendarServiceImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeOptimizationGuideService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    fake_optimization_guide_service_ =
        static_cast<FakeOptimizationGuideService*>(
            OptimizationGuideServiceFactory::GetForProfile(profile_.get()));

    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetBrowserState(profile_.get());

    // Sign in a user.
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        "test@example.com", signin::ConsentLevel::kSignin);

    service_ = std::make_unique<EnhancedCalendarServiceImpl>(
        remote_.BindNewPipeAndPassReceiver(), fake_web_state_.get());
  }

  void TearDown() override {
    service_.reset();
    fake_web_state_.reset();
    PlatformTest::TearDown();
  }

  void CallOnPageContextGenerated(
      optimization_guide::proto::EnhancedCalendarRequest request,
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> response) {
    service_->OnPageContextGenerated(std::move(request), std::move(response));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  raw_ptr<FakeOptimizationGuideService> fake_optimization_guide_service_;
  base::MockCallback<
      base::OnceCallback<void(mojom::EnhancedCalendarResponseResultPtr)>>
      mock_callback_;
  std::unique_ptr<EnhancedCalendarServiceImpl> service_;
  mojo::Remote<mojom::EnhancedCalendarService> remote_;
};

namespace {

// Tests that the service can be instantiated.
TEST_F(EnhancedCalendarServiceImplTest, ServiceCreated) {
  EXPECT_NE(service_, nullptr);
}

// Tests that a pending request is cancelled when the user signs out.
TEST_F(EnhancedCalendarServiceImplTest, CancelOnIdentityChange) {
  EXPECT_CALL(mock_callback_, Run(testing::_))
      .WillOnce([](mojom::EnhancedCalendarResponseResultPtr result) {
        EXPECT_TRUE(result->is_error());
      });

  mojom::EnhancedCalendarServiceRequestParamsPtr params =
      mojom::EnhancedCalendarServiceRequestParams::New();
  params->selected_text = "dinner at 7pm";
  params->surrounding_text = "I have dinner at 7pm tonight.";

  service_->ExecuteEnhancedCalendarRequest(std::move(params),
                                           mock_callback_.Get());

  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
}

// Tests that OnPageContextGenerated calls ExecuteModel with correct request.
TEST_F(EnhancedCalendarServiceImplTest,
       OnPageContextGeneratedCallsExecuteModel) {
  EXPECT_CALL(mock_callback_, Run(testing::_))
      .WillOnce([](mojom::EnhancedCalendarResponseResultPtr result) {
        EXPECT_TRUE(result->is_error());
      });

  // We need to set the pending callback so that OnPageContextGenerated doesn't
  // return early.
  mojom::EnhancedCalendarServiceRequestParamsPtr params =
      mojom::EnhancedCalendarServiceRequestParams::New();
  service_->ExecuteEnhancedCalendarRequest(std::move(params),
                                           mock_callback_.Get());

  optimization_guide::proto::EnhancedCalendarRequest request;
  request.set_selected_text("dinner at 7pm");

  auto page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url("https://example.com");

  base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                 PageContextWrapperError>
      response(std::move(page_context));

  CallOnPageContextGenerated(std::move(request), std::move(response));

  // Verify that ExecuteModel was called.
  EXPECT_EQ(fake_optimization_guide_service_->feature_,
            optimization_guide::ModelBasedCapabilityKey::kEnhancedCalendar);

  EXPECT_NE(fake_optimization_guide_service_->last_request_, nullptr);

  // Cast the last request back to EnhancedCalendarRequest.
  const optimization_guide::proto::EnhancedCalendarRequest* sent_request =
      static_cast<const optimization_guide::proto::EnhancedCalendarRequest*>(
          fake_optimization_guide_service_->last_request_.get());

  EXPECT_EQ(sent_request->selected_text(), "dinner at 7pm");
  EXPECT_EQ(sent_request->page_context().url(), "https://example.com");
}

}  // namespace

}  // namespace ai
