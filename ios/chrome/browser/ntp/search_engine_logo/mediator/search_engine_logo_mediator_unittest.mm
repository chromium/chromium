// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_provider_logos/logo_common.h"
#import "ios/chrome/browser/google/model/google_logo_service.h"
#import "ios/chrome/browser/google/model/google_logo_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "url/gurl.h"

namespace {

using ::testing::_;
using ::testing::Return;

class MockGoogleLogoService : public GoogleLogoService {
 public:
  MockGoogleLogoService(TemplateURLService* template_url_service,
                        signin::IdentityManager* identity_manager)
      : GoogleLogoService(template_url_service, identity_manager, nullptr) {}

  MOCK_METHOD2(GetLogo, void(search_provider_logos::LogoCallbacks, bool));
};

std::unique_ptr<KeyedService> BuildMockGoogleLogoService(ProfileIOS* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<MockGoogleLogoService>(
      ios::TemplateURLServiceFactory::GetForProfile(profile), identity_manager);
}

class SearchEngineLogoMediatorTest : public PlatformTest {
 protected:
  SearchEngineLogoMediatorTest()
      : web_state_(std::make_unique<web::FakeWebState>()) {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        GoogleLogoServiceFactory::GetInstance(),
        base::BindOnce(&BuildMockGoogleLogoService));
    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    logo_service_ = static_cast<MockGoogleLogoService*>(
        GoogleLogoServiceFactory::GetForProfile(profile_.get()));
    UrlLoadingBrowserAgent* URLLoadingBrowserAgent =
        UrlLoadingBrowserAgent::FromBrowser(browser_.get());
    scoped_refptr<network::SharedURLLoaderFactory> sharedURLLoaderFactory =
        profile_->GetSharedURLLoaderFactory();
    BOOL offTheRecord = profile_->IsOffTheRecord();
    EXPECT_CALL(*logo_service_, GetLogo(_, false));
    mediator_ = [[SearchEngineLogoMediator alloc]
              initWithWebState:web_state_.get()
            templateURLService:template_url_service
                   logoService:logo_service_
        URLLoadingBrowserAgent:URLLoadingBrowserAgent
        sharedURLLoaderFactory:sharedURLLoaderFactory
                  offTheRecord:offTheRecord];

    std::unique_ptr<search_engines::ChoiceScreenData> choice_screen_data =
        template_url_service->GetChoiceScreenData();
    const TemplateURL::OwnedTemplateURLVector& search_engines =
        choice_screen_data->search_engines();
    for (size_t i = 0; i < search_engines.size(); ++i) {
      const std::unique_ptr<TemplateURL>& template_url = search_engines[i];
      if (template_url->keyword() != google_keyword_) {
        other_search_engine_keyword_ = template_url->keyword();
        break;
      }
    }
  }

  void SelectSearchEngineWithKeyword(std::u16string keyword) {
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    TemplateURL* selected_template_url = nullptr;
    std::unique_ptr<search_engines::ChoiceScreenData> choice_screen_data =
        template_url_service->GetChoiceScreenData();
    search_engines::ChoiceScreenDisplayState display_state =
        choice_screen_data->display_state();
    const TemplateURL::OwnedTemplateURLVector& search_engines =
        choice_screen_data->search_engines();
    for (size_t i = 0; i < search_engines.size(); ++i) {
      auto& template_url = search_engines[i];
      if (template_url->keyword() == keyword) {
        selected_template_url = template_url.get();
        display_state.selected_engine_index = i;
        break;
      }
    }
    CHECK(selected_template_url);
    template_url_service->SetUserSelectedDefaultSearchProvider(
        selected_template_url,
        search_engines::ChoiceMadeLocation::kChoiceScreen);
    [mediator_ searchEngineChanged];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  raw_ptr<MockGoogleLogoService> logo_service_;
  SearchEngineLogoMediator* mediator_;
  std::u16string google_keyword_ = u"google.com";
  std::u16string other_search_engine_keyword_;
};

// Sanity check.
TEST_F(SearchEngineLogoMediatorTest, TestConstructorDestructor) {
  EXPECT_TRUE(mediator_);
}

// Verifies that tapping the doodle navigates to the stored URL.
TEST_F(SearchEngineLogoMediatorTest, TestTapDoodleWithValidClickURL) {
  const std::string kURL = "http://foo/";
  [mediator_ setClickURLText:GURL(kURL)];

  // Tap the doodle and verify the expected url was loaded.
  [mediator_ simulateDoodleTapped];
  EXPECT_EQ(kURL, url_loader_->last_params.web_params.url);
  EXPECT_EQ(1, url_loader_->load_current_tab_call_count);
}

// Verifies the case where the URL value is invalid (which should result in not
// attempting to load any URL when the doodle is tapped).
TEST_F(SearchEngineLogoMediatorTest, TestTapDoodle_InvalidSearchQuery) {
  [mediator_ setClickURLText:GURL("foo")];

  // Tap the doodle and verify nothing was loaded.
  [mediator_ simulateDoodleTapped];

  EXPECT_EQ(GURL(), url_loader_->last_params.web_params.url);
  EXPECT_EQ(0, url_loader_->load_current_tab_call_count);
}

// Verifies that the logo fetch is not restarted when the fetch is failed.
TEST_F(SearchEngineLogoMediatorTest, TestFetchNotRestartedWhenFailed) {
  // Set to other search engine.
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(testing::AtMost(1));
  SelectSearchEngineWithKeyword(other_search_engine_keyword_);
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(0);

  // Switch to Google search engine to trigger a doodle fetch.
  search_provider_logos::LogoCallback logo_callback;
  EXPECT_CALL(*logo_service_, GetLogo(_, false))
      .WillOnce([&logo_callback](search_provider_logos::LogoCallbacks callbacks,
                                 bool for_doodle) {
        logo_callback = std::move(callbacks.on_fresh_decoded_logo_available);
      });
  SelectSearchEngineWithKeyword(google_keyword_);
  std::move(logo_callback)
      .Run(search_provider_logos::LogoCallbackReason::FAILED, std::nullopt);
  // Verify that the logo fetch is not restarted.
  base::RunLoop run_loop;
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(0);
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();
}

// Verifies that the logo fetch is restarted when the fetch is canceled.
TEST_F(SearchEngineLogoMediatorTest, TestFetchRestartedWhenCanceled) {
  // Set to other search engine.
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(testing::AtMost(1));
  SelectSearchEngineWithKeyword(other_search_engine_keyword_);
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(0);

  // Switch to Google search engine to trigger a doodle fetch.
  // Expect one call, which will be "canceled".
  search_provider_logos::LogoCallback logo_callback;
  EXPECT_CALL(*logo_service_, GetLogo(_, false))
      .WillOnce([&logo_callback](search_provider_logos::LogoCallbacks callbacks,
                                 bool for_doodle) {
        logo_callback = std::move(callbacks.on_fresh_decoded_logo_available);
      });
  SelectSearchEngineWithKeyword(google_keyword_);
  std::move(logo_callback)
      .Run(search_provider_logos::LogoCallbackReason::CANCELED, std::nullopt);
  // Verify that the logo fetch is restarted.
  base::RunLoop run_loop;
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).WillOnce([&run_loop] {
    run_loop.Quit();
  });
  run_loop.Run();
}

// Tests that there is no crash if the mediator is disconnected during a logo
// fetch.
TEST_F(SearchEngineLogoMediatorTest, TestDisconnectMediatorWhileFetching) {
  // Set to other search engine.
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(testing::AtMost(1));
  SelectSearchEngineWithKeyword(other_search_engine_keyword_);
  EXPECT_CALL(*logo_service_, GetLogo(_, false)).Times(0);

  search_provider_logos::LogoCallback logo_callback;
  EXPECT_CALL(*logo_service_, GetLogo(_, false))
      .WillOnce([&logo_callback](search_provider_logos::LogoCallbacks callbacks,
                                 bool for_doodle) {
        logo_callback = std::move(callbacks.on_fresh_decoded_logo_available);
      });
  SelectSearchEngineWithKeyword(google_keyword_);
  // Disconnect first, and then call the callback.
  [mediator_ disconnect];
  search_provider_logos::Logo logo;
  std::move(logo_callback)
      .Run(search_provider_logos::LogoCallbackReason::DETERMINED, logo);
}

}  // anonymous namespace
