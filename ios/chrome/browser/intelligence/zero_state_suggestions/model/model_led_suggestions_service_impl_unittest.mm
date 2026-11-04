// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/model_led_suggestions_service_impl.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper_factory.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/fake_optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace ai {

namespace {

// Helper function to create a FakeOptimizationGuideService.
std::unique_ptr<KeyedService> CreateFakeOptimizationGuideService(
    ProfileIOS* profile) {
  return std::make_unique<FakeOptimizationGuideService>(
      profile->GetProtoDatabaseProvider(), profile->GetStatePath(),
      profile->IsOffTheRecord(), "en",
      nullptr,  // hint_store
      profile->GetPrefs(), BrowserListFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace

class ModelLedSuggestionsServiceImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Set up the profile builder with a fake OptimizationGuideService.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeOptimizationGuideService));
    profile_ = std::move(builder).Build();

    // Cache the fake service pointer for testing.
    fake_optimization_guide_service_ =
        static_cast<FakeOptimizationGuideService*>(
            OptimizationGuideServiceFactory::GetForProfile(profile_.get()));

    // Set up a fake WebState.
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetBrowserState(profile_.get());
    fake_web_state_->SetVisibleURL(GURL("https://example.com"));

    // Initialize the service under test.
    service_ = std::make_unique<ModelLedSuggestionsServiceImpl>(
        remote_.BindNewPipeAndPassReceiver(), fake_web_state_.get(),
        &fake_page_context_wrapper_factory_);
  }

  void CompletePageContextExtraction() {
    FakePageContextWrapper* wrapper =
        fake_page_context_wrapper_factory_.last_wrapper();
    ASSERT_NE(nil, wrapper);
    EXPECT_TRUE(wrapper.shouldGetInnerText);
    EXPECT_TRUE(wrapper.populateCalled);
    auto page_context =
        std::make_unique<optimization_guide::proto::PageContext>();
    page_context->set_url("https://example.com");
    page_context->set_title("Example");
    [wrapper completeWithPageContext:std::move(page_context)];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  FakePageContextWrapperFactory fake_page_context_wrapper_factory_;
  std::unique_ptr<ModelLedSuggestionsServiceImpl> service_;
  mojo::Remote<mojom::ModelLedSuggestionsService> remote_;
  raw_ptr<FakeOptimizationGuideService> fake_optimization_guide_service_;
};

// Tests that an error is returned if the WebState is destroyed before fetching.
TEST_F(ModelLedSuggestionsServiceImplTest,
       TestFetchSuggestionsWebStateDestroyed) {
  fake_web_state_.reset();

  base::test::TestFuture<ai::mojom::ModelLedSuggestionsResponseResultPtr>
      future;
  service_->FetchModelLedSuggestions(future.GetCallback());

  EXPECT_TRUE(future.Get()->is_error());
  EXPECT_EQ(future.Get()->get_error(), "WebState destroyed.");
}

// Tests that a successful model execution returns a response.
TEST_F(ModelLedSuggestionsServiceImplTest, TestFetchSuggestionsSuccess) {
  optimization_guide::proto::ZeroStateSuggestionsResponse response;

  fake_optimization_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      response, "optimization_guide.proto.ZeroStateSuggestionsResponse");

  base::test::TestFuture<ai::mojom::ModelLedSuggestionsResponseResultPtr>
      future;
  service_->FetchModelLedSuggestions(future.GetCallback());
  CompletePageContextExtraction();

  EXPECT_TRUE(future.Get()->is_response());
}

// Tests that an error is returned when the model execution fails.
TEST_F(ModelLedSuggestionsServiceImplTest, TestFetchSuggestionsFailure) {
  fake_optimization_guide_service_->SetError(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      static_cast<int>(
          optimization_guide::OptimizationGuideModelExecutionError::
              ModelExecutionError::kGenericFailure));

  base::test::TestFuture<ai::mojom::ModelLedSuggestionsResponseResultPtr>
      future;
  service_->FetchModelLedSuggestions(future.GetCallback());
  CompletePageContextExtraction();

  EXPECT_TRUE(future.Get()->is_error());
  EXPECT_EQ(future.Get()->get_error(), "Server model execution error.");
}

}  // namespace ai
