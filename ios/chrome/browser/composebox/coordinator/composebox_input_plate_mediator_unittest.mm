// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"

#import "base/no_destructor.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/contextual_search/contextual_search_context_controller.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/internal/test_composebox_query_controller.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/prefs/testing_pref_service.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_test_util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/variations/variations_client.h"
#import "components/version_info/channel.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Mock consumer for the mediator.
@interface TestComposeboxInputPlateConsumer
    : NSObject <ComposeboxInputPlateConsumer>

@property(nonatomic, assign) BOOL eligibleToAIMode;
@property(nonatomic, assign) BOOL showsSendButton;
@property(nonatomic, assign) BOOL showsExtendedControls;

@end

@implementation TestComposeboxInputPlateConsumer

- (void)setItems:(NSArray<ComposeboxInputItem*>*)items {
}
- (void)updateState:(ComposeboxInputItemState)state
    forItemWithIdentifier:(const base::UnguessableToken&)identifier {
}

- (void)setEligibleToAIMode:(BOOL)eligibleToAIMode {
  _eligibleToAIMode = eligibleToAIMode;
}

- (void)setShowsSendButton:(BOOL)showsSendButton {
  _showsSendButton = showsSendButton;
}

- (void)setShowsExtendedControls:(BOOL)showsExtendedControls {
  _showsExtendedControls = showsExtendedControls;
}

- (void)setAIModeEnabled:(BOOL)enabled {
}
- (void)setImageGenerationEnabled:(BOOL)enabled {
}
- (void)setCompact:(BOOL)compact {
}
- (void)setCurrentTabFavicon:(UIImage*)favicon {
}
- (void)hideAttachCurrentTabAction:(BOOL)hidden {
}
- (void)hideAttachTabActions:(BOOL)hidden {
}
- (void)disableAttachTabActions:(BOOL)disabled {
}
- (void)hideAttachFileActions:(BOOL)hidden {
}
- (void)disableAttachFileActions:(BOOL)disabled {
}
- (void)hideCreateImageActions:(BOOL)hidden {
}
- (void)disableCreateImageActions:(BOOL)disabled {
}
- (void)disableCameraActions:(BOOL)disabled {
}
- (void)disableGalleryActions:(BOOL)disabled {
}

@end

namespace {

class ComposeboxInputPlateMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    omnibox::RegisterProfilePrefs(pref_service_.registry());
    AimEligibilityService::RegisterProfilePrefs(pref_service_.registry());
    profile_ = TestProfileIOS::Builder().Build();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    fake_variations_client_ = std::make_unique<FakeVariationsClient>();
    service_ = std::make_unique<contextual_search::ContextualSearchService>(
        nullptr, shared_url_loader_factory_, template_url_service(),
        fake_variations_client_.get(), version_info::Channel::STABLE, "en-US");
    auto config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    static base::NoDestructor<network::TestURLLoaderFactory>
        test_url_loader_factory;
    aim_eligibility_service_ =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            pref_service_, template_url_service(),
            test_url_loader_factory->GetSafeWeakWrapper(),
            IdentityManagerFactory::GetForProfile(profile_.get()));
    EXPECT_CALL(*aim_eligibility_service_,
                RegisterEligibilityChangedCallback(testing::_))
        .WillOnce(
            testing::DoAll(testing::SaveArg<0>(&aim_callback_),
                           testing::Return(base::CallbackListSubscription())));
    mediator_ = [[ComposeboxInputPlateMediator alloc]
        initWithContextualSearchSession:
            service_->CreateSession(
                std::move(config_params),
                contextual_search::ContextualSearchSource::kUnknown)
                           webStateList:nullptr
                          faviconLoader:nullptr
                 persistTabContextAgent:nullptr
                            isIncognito:NO
                             modeHolder:[[ComposeboxModeHolder alloc] init]
                     templateURLService:template_url_service()
                  aimEligibilityService:aim_eligibility_service_.get()];
    consumer_ = [[TestComposeboxInputPlateConsumer alloc] init];
    mediator_.consumer = consumer_;

    template_url_service()->Load();
    TemplateURLServiceLoadWaiter waiter;
    waiter.WaitForLoadComplete(*template_url_service());
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    consumer_ = nil;
    aim_eligibility_service_.reset();
    service_.reset();
    fake_variations_client_.reset();
    shared_url_loader_factory_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  TemplateURLService* template_url_service() {
    return search_engines_test_environment_.template_url_service();
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
  std::unique_ptr<testing::NiceMock<MockAimEligibilityService>>
      aim_eligibility_service_;
  base::RepeatingClosure aim_callback_;
  TestComposeboxInputPlateConsumer* consumer_;
  ComposeboxInputPlateMediator* mediator_;
};

// Tests that the consumer is informed when AIM is eligible.
TEST_F(ComposeboxInputPlateMediatorTest, InformConsumerWhenAimEligible) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  ASSERT_FALSE(aim_callback_.is_null());

  aim_callback_.Run();

  EXPECT_TRUE(consumer_.eligibleToAIMode);
}

// Tests that the consumer is informed when AIM is not eligible.
TEST_F(ComposeboxInputPlateMediatorTest, InformConsumerWhenAimNotEligible) {
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(false));
  ASSERT_FALSE(aim_callback_.is_null());

  aim_callback_.Run();

  EXPECT_FALSE(consumer_.eligibleToAIMode);
}

// Tests that extended controls are shown when Google is the default search
// engine.
TEST_F(ComposeboxInputPlateMediatorTest, ShowsExtendedControlsWithGoogleDSE) {
  TemplateURLService* template_url_service = this->template_url_service();
  TemplateURLData data;
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  data.safe_for_autoreplace = true;
  data.prepopulate_id = 1;
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_TRUE(consumer_.showsExtendedControls);
}

// Tests that extended controls are hidden when Google is not the default search
// engine.
TEST_F(ComposeboxInputPlateMediatorTest,
       HidesExtendedControlsWithNonGoogleDSE) {
  TemplateURLService* template_url_service = this->template_url_service();
  TemplateURLData data;
  data.SetURL("https://www.bing.com/search?q={searchTerms}");
  data.safe_for_autoreplace = false;
  data.prepopulate_id = 2;
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_FALSE(consumer_.showsExtendedControls);
}

// Tests that the send button is shown when there is text in the omnibox.
TEST_F(ComposeboxInputPlateMediatorTest, ShowsSendButtonWithText) {
  [mediator_ omniboxDidChangeText:u"some text"
                    isSearchQuery:NO
              userInputInProgress:NO];

  EXPECT_TRUE(consumer_.showsSendButton);
}

// Tests that the send button is hidden when there is no text in the omnibox.
TEST_F(ComposeboxInputPlateMediatorTest, HidesSendButtonWithoutText) {
  [mediator_ omniboxDidChangeText:u"" isSearchQuery:NO userInputInProgress:NO];

  EXPECT_FALSE(consumer_.showsSendButton);
}

}  // namespace
