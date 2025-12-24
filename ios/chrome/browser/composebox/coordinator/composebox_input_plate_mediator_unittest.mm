// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"

#import "base/no_destructor.h"
#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
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
#import "ios/chrome/browser/composebox/public/composebox_input_plate_controls.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Mock consumer for the mediator.
@interface TestComposeboxInputPlateConsumer
    : NSObject <ComposeboxInputPlateConsumer>

// Whether the given control(s) are shown.
- (BOOL)showsControls:(ComposeboxInputPlateControls)controls;

@end

@implementation TestComposeboxInputPlateConsumer {
  ComposeboxInputPlateControls _visibleControls;
}

- (void)setItems:(NSArray<ComposeboxInputItem*>*)items {
}
- (void)updateState:(ComposeboxInputItemState)state
    forItemWithIdentifier:(const base::UnguessableToken&)identifier {
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
- (void)hideCameraActions:(BOOL)hidden {
}
- (void)disableCameraActions:(BOOL)disabled {
}
- (void)hideGalleryActions:(BOOL)hidden {
}
- (void)disableGalleryActions:(BOOL)disabled {
}
- (void)updateVisibleControls:(ComposeboxInputPlateControls)visibleControls {
  _visibleControls = visibleControls;
}

- (BOOL)showsControls:(ComposeboxInputPlateControls)controls {
  return (_visibleControls & controls) != ComposeboxInputPlateControls::kNone;
}

@end

namespace {

class ComposeboxInputPlateMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    omnibox::RegisterProfilePrefs(pref_service_.registry());
    contextual_search::ContextualSearchService::RegisterProfilePrefs(
        pref_service_.registry());
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

    web_state_list_delegate_ = std::make_unique<FakeWebStateListDelegate>();
    web_state_list_ =
        std::make_unique<WebStateList>(web_state_list_delegate_.get());
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());

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
    auto session_handle = service_->CreateSession(
        std::move(config_params),
        contextual_search::ContextualSearchSource::kUnknown);
    // Check the search content sharing settings to notify the session handle
    // that the client is properly checking the pref value.
    session_handle->CheckSearchContentSharingSettings(&pref_service_);
    mediator_ = [[ComposeboxInputPlateMediator alloc]
        initWithContextualSearchSession:std::move(session_handle)
                           webStateList:web_state_list_.get()
                          faviconLoader:nullptr
                 persistTabContextAgent:nullptr
                            isIncognito:NO
                             modeHolder:[[ComposeboxModeHolder alloc] init]
                     templateURLService:template_url_service()
                  aimEligibilityService:aim_eligibility_service_.get()
                            prefService:&pref_service_];
    consumer_ = [[TestComposeboxInputPlateConsumer alloc] init];
    mediator_.consumer = consumer_;

    template_url_service()->Load();
    TemplateURLServiceLoadWaiter waiter;
    waiter.WaitForLoadComplete(*template_url_service());
    EnableInputPlateFeatures({});
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    consumer_ = nil;
    aim_eligibility_service_.reset();
    service_.reset();
    fake_variations_client_.reset();
    shared_url_loader_factory_.reset();
    web_state_list_.reset();
    web_state_list_delegate_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

 protected:
  struct InputPlateFeatures {
    bool compactMode;
    bool aimNudge;
  };

  TemplateURLService* template_url_service() {
    return search_engines_test_environment_.template_url_service();
  }

  void SetDSEGoogle(bool isGoogleDSE) {
    TemplateURLService* template_url_service = this->template_url_service();
    TemplateURLData data;

    if (isGoogleDSE) {
      data.SetURL("https://www.google.com/search?q={searchTerms}");
      data.safe_for_autoreplace = true;
      data.prepopulate_id = 1;
    } else {
      data.SetURL("https://www.bing.com/search?q={searchTerms}");
      data.safe_for_autoreplace = false;
      data.prepopulate_id = 2;
    }

    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void SetAIMEligible(bool AIMEligible) {
    EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
        .WillRepeatedly(testing::Return(AIMEligible));
    ASSERT_FALSE(aim_callback_.is_null());

    aim_callback_.Run();
  }

  void SetOmniboxText(const std::u16string& text) {
    [mediator_ omniboxDidChangeText:text
                      isSearchQuery:NO
                userInputInProgress:NO];
  }

  void EraseOmniboxText() { SetOmniboxText(u""); }

  void EnableInputPlateFeatures(InputPlateFeatures features) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (features.compactMode) {
      enabled_features.push_back(kComposeboxCompactMode);
    } else {
      disabled_features.push_back(kComposeboxCompactMode);
    }

    if (features.aimNudge) {
      enabled_features.push_back(kComposeboxAIMNudge);
    } else {
      disabled_features.push_back(kComposeboxAIMNudge);
    }

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
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
  std::unique_ptr<FakeWebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  TestComposeboxInputPlateConsumer* consumer_;
  ComposeboxInputPlateMediator* mediator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ComposeboxInputPlateMediatorTest, ShowsSendButtonWithAttachments) {
  SetAIMEligible(true);
  SetDSEGoogle(true);
  EraseOmniboxText();
  EXPECT_FALSE([consumer_ showsControls:ComposeboxInputPlateControls::kSend]);
  UIImage* image = [[UIImage alloc] init];
  NSItemProvider* provider = [[NSItemProvider alloc] initWithObject:image];
  [mediator_ processImageItemProvider:provider assetID:@"123"];
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kSend]);
}

// Disables multimodal options when not eligible.
TEST_F(ComposeboxInputPlateMediatorTest,
       DisablesMultimodalActionsWhenAIMNotEligible) {
  SetAIMEligible(false);
  SetDSEGoogle(true);
  EXPECT_TRUE(
      [consumer_ showsControls:ComposeboxInputPlateControls::kLeadingImage]);
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kVoice]);
  EXPECT_FALSE([consumer_ showsControls:ComposeboxInputPlateControls::kPlus]);
}

// Tests that extended controls are shown when Google is the default search
// engine.
TEST_F(ComposeboxInputPlateMediatorTest, ShowsExtendedControlsWithGoogleDSE) {
  SetAIMEligible(true);
  SetDSEGoogle(true);
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kVoice]);
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kPlus]);
}

// Tests that extended controls are hidden when Google is not the default search
// engine.
TEST_F(ComposeboxInputPlateMediatorTest,
       HidesExtendedControlsWithNonGoogleDSE) {
  SetAIMEligible(true);
  SetDSEGoogle(false);
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kVoice]);
  EXPECT_TRUE(
      [consumer_ showsControls:ComposeboxInputPlateControls::kLeadingImage]);
  EXPECT_FALSE([consumer_ showsControls:ComposeboxInputPlateControls::kPlus]);
}

// Tests that the send button is shown when there is text in the omnibox.
TEST_F(ComposeboxInputPlateMediatorTest, ShowsSendButtonWithText) {
  SetOmniboxText(u"some text");
  SetAIMEligible(true);
  SetDSEGoogle(true);
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kSend]);
}

// Tests that the send button is hidden when there is no text in the omnibox.
TEST_F(ComposeboxInputPlateMediatorTest, HidesSendButtonWithoutText) {
  EraseOmniboxText();
  SetAIMEligible(true);
  SetDSEGoogle(true);
  EXPECT_FALSE([consumer_ showsControls:ComposeboxInputPlateControls::kSend]);
}

// Tests that the leading image is hidden when in compact mode with Google DSE.
TEST_F(ComposeboxInputPlateMediatorTest,
       HidesLeadingImageForCompactModeWithGoogleDSE) {
  EnableInputPlateFeatures({
      .compactMode = true,
  });

  SetAIMEligible(true);
  SetDSEGoogle(true);
  // A text short enough it does not wrap and leds to compact mode.
  SetOmniboxText(u"some text");

  EXPECT_FALSE(
      [consumer_ showsControls:ComposeboxInputPlateControls::kLeadingImage]);
  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kPlus]);
}

//
TEST_F(ComposeboxInputPlateMediatorTest, TestsAIMNudgeShownWithGoogleDSE) {
  EnableInputPlateFeatures({.aimNudge = true});

  SetAIMEligible(true);
  SetDSEGoogle(true);
  SetOmniboxText(u"some text");

  EXPECT_TRUE([consumer_ showsControls:ComposeboxInputPlateControls::kAIM]);
}

//
TEST_F(ComposeboxInputPlateMediatorTest,
       TestsAIMNudgeNotShownWithDifferentDSE) {
  EnableInputPlateFeatures({.aimNudge = true});

  SetAIMEligible(true);
  SetDSEGoogle(false);
  SetOmniboxText(u"some text");

  EXPECT_FALSE([consumer_ showsControls:ComposeboxInputPlateControls::kAIM]);
}

// Tests that QR code button is shown with non Google DSE.
TEST_F(ComposeboxInputPlateMediatorTest, ShowsQRScannerButtonWithNonGoogleDSE) {
  SetAIMEligible(false);
  SetDSEGoogle(false);
  EXPECT_TRUE(
      [consumer_ showsControls:ComposeboxInputPlateControls::kQRScanner]);
  EXPECT_FALSE([consumer_ showsControls:ComposeboxInputPlateControls::kLens]);
}

}  // namespace
