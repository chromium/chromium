// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/zero_state_suggestions_service_impl.h"

#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/fake_optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

@interface ZeroStateSuggestionsFakePageContextWrapper : PageContextWrapper
@end

@implementation ZeroStateSuggestionsFakePageContextWrapper {
  base::OnceCallback<void(PageContextWrapperCallbackResponse)> _callback;
}

- (instancetype)initWithWebState:(web::WebState*)webState
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  self = [super initWithWebState:webState completionCallback:base::DoNothing()];
  if (self) {
    _callback = std::move(completionCallback);
  }
  return self;
}

- (void)populatePageContextFieldsAsync {
  auto page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url("https://example.com");
  page_context->set_title("Example");
  std::move(_callback).Run(base::ok(std::move(page_context)));
}
@end

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

class ZeroStateSuggestionsServiceImplTest : public PlatformTest {
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
    service_ = std::make_unique<ZeroStateSuggestionsServiceImpl>(
        remote_.BindNewPipeAndPassReceiver(), fake_web_state_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<ZeroStateSuggestionsServiceImpl> service_;
  mojo::Remote<mojom::ZeroStateSuggestionsService> remote_;
  raw_ptr<FakeOptimizationGuideService> fake_optimization_guide_service_;
};

// Tests that an error is returned if the WebState is destroyed before fetching.
TEST_F(ZeroStateSuggestionsServiceImplTest,
       TestFetchSuggestionsWebStateDestroyed) {
  fake_web_state_.reset();

  base::test::TestFuture<ai::mojom::ZeroStateSuggestionsResponseResultPtr>
      future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  EXPECT_TRUE(future.Get()->is_error());
  EXPECT_EQ(future.Get()->get_error(), "WebState destroyed.");
}

// Tests that a successful model execution returns a response.
TEST_F(ZeroStateSuggestionsServiceImplTest, TestFetchSuggestionsSuccess) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  OCMStub([mockWrapperClass alloc])
      .andReturn([ZeroStateSuggestionsFakePageContextWrapper alloc]);

  optimization_guide::proto::ZeroStateSuggestionsResponse response;

  fake_optimization_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      response, "optimization_guide.proto.ZeroStateSuggestionsResponse");

  base::test::TestFuture<ai::mojom::ZeroStateSuggestionsResponseResultPtr>
      future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  EXPECT_TRUE(future.Get()->is_response());
}

// Tests that an error is returned when the model execution fails.
TEST_F(ZeroStateSuggestionsServiceImplTest, TestFetchSuggestionsFailure) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  OCMStub([mockWrapperClass alloc])
      .andReturn([ZeroStateSuggestionsFakePageContextWrapper alloc]);

  fake_optimization_guide_service_->SetError(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      static_cast<int>(
          optimization_guide::OptimizationGuideModelExecutionError::
              ModelExecutionError::kGenericFailure));

  base::test::TestFuture<ai::mojom::ZeroStateSuggestionsResponseResultPtr>
      future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  EXPECT_TRUE(future.Get()->is_error());
  EXPECT_EQ(future.Get()->get_error(), "Server model execution error.");
}

}  // namespace ai
