// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/contextual_cueing/contextual_cueing_enums.h"
#import "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#import "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service_factory.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/fake_optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace contextual_cueing {

namespace {

std::unique_ptr<KeyedService> BuildFakeGeminiService(ProfileIOS* profile) {
  auto fake_service = std::make_unique<FakeGeminiService>();
  fake_service->SetIsEligible(true);
  return fake_service;
}

std::unique_ptr<KeyedService> BuildTestInProcessClassificationService(
    ProfileIOS* profile) {
  static base::NoDestructor<
      optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider;
  return std::make_unique<InProcessCategoryClassificationService>(
      test_model_provider.get());
}

class FakeOnDevicePageClassificationService
    : public OnDevicePageClassificationService {
 public:
  explicit FakeOnDevicePageClassificationService(
      InProcessCategoryClassificationService* in_process_classifier)
      : OnDevicePageClassificationService(in_process_classifier) {}
  ~FakeOnDevicePageClassificationService() override = default;

  void ClassifyWebState(web::WebState* web_state,
                        PageClassificationCallback callback) override {
    last_classified_web_state_id_ =
        web_state ? web_state->GetUniqueIdentifier() : web::WebStateID();
    pending_callback_ = std::move(callback);
    if (auto_respond_) {
      RespondWithCategories(canned_categories_);
    }
  }

  void CancelClassification(web::WebState* web_state) override {
    if (web_state) {
      cancelled_web_state_ids_.push_back(web_state->GetUniqueIdentifier());
    }
    pending_callback_.Reset();
  }

  void SetCannedCategories(
      std::optional<std::vector<page_content_annotations::Category>>
          categories) {
    canned_categories_ = std::move(categories);
  }

  void RespondWithCategories(
      const std::optional<std::vector<page_content_annotations::Category>>&
          categories) {
    if (pending_callback_) {
      std::move(pending_callback_).Run(categories);
    }
  }

  void Shutdown() override {
    last_classified_web_state_id_ = web::WebStateID();
    cancelled_web_state_ids_.clear();
    pending_callback_.Reset();
    OnDevicePageClassificationService::Shutdown();
  }

  void set_auto_respond(bool auto_respond) { auto_respond_ = auto_respond; }

  web::WebStateID last_classified_web_state_id_;
  std::vector<web::WebStateID> cancelled_web_state_ids_;
  PageClassificationCallback pending_callback_;
  bool auto_respond_ = true;
  std::optional<std::vector<page_content_annotations::Category>>
      canned_categories_;
};

std::unique_ptr<KeyedService> BuildTestPageClassificationService(
    ProfileIOS* profile) {
  InProcessCategoryClassificationService* in_process_service =
      InProcessCategoryClassificationServiceFactory::GetForProfile(profile);
  return std::make_unique<FakeOnDevicePageClassificationService>(
      in_process_service);
}

std::unique_ptr<KeyedService> CreateFakeOptimizationGuideService(
    ProfileIOS* profile) {
  return std::make_unique<FakeOptimizationGuideService>(
      profile->GetProtoDatabaseProvider(), profile->GetStatePath(),
      profile->IsOffTheRecord(), "en",
      base::WeakPtr<optimization_guide::OptimizationGuideStore>(),
      profile->GetPrefs(), nullptr, nullptr,
      IdentityManagerFactory::GetForProfile(profile));
}

class TestCueingObserver : public ContextualCueingTabHelper::Observer {
 public:
  void OnPageClassificationCompleted(
      ContextualCueingTabHelper* tab_helper,
      const std::optional<std::vector<page_content_annotations::Category>>&
          categories) override {
    notified_tab_helper_ = tab_helper;
    notified_categories_ = categories;
    call_count_++;
  }

  void OnContextualCueReceived(
      ContextualCueingTabHelper* tab_helper,
      const std::optional<optimization_guide::proto::ContextualCue>& cue)
      override {
    notified_tab_helper_ = tab_helper;
    notified_cue_ = cue;
    cue_call_count_++;
  }

  void OnContextualCueInvalidated(
      ContextualCueingTabHelper* tab_helper) override {
    notified_tab_helper_ = tab_helper;
    invalidated_call_count_++;
  }

  raw_ptr<ContextualCueingTabHelper> notified_tab_helper_ = nullptr;
  std::optional<std::vector<page_content_annotations::Category>>
      notified_categories_;
  std::optional<optimization_guide::proto::ContextualCue> notified_cue_;
  int call_count_ = 0;
  int cue_call_count_ = 0;
  int invalidated_call_count_ = 0;
};

class FakeContextualCueingDelegate
    : public ContextualCueingTabHelper::Delegate {
 public:
  std::vector<ContextualCueingTabHelper::BackgroundTabContext>
  GetEligibleBackgroundTabs(web::WebState* active_web_state,
                            size_t max_tabs) override {
    return background_tabs_;
  }

  void SetBackgroundTabs(
      std::vector<ContextualCueingTabHelper::BackgroundTabContext>
          background_tabs) {
    background_tabs_ = std::move(background_tabs);
  }

 private:
  std::vector<ContextualCueingTabHelper::BackgroundTabContext> background_tabs_;
};

optimization_guide::proto::ContextualCueingResponse CreateTestCueResponse(
    const std::string& anchored_message_text,
    const std::string& action_text) {
  optimization_guide::proto::ContextualCueingResponse response;
  optimization_guide::proto::ContextualCue* cue =
      response.add_contextual_cues();
  cue->mutable_gemini_in_chrome_surface();
  cue->mutable_anchored_message_cue()->set_anchored_message_text(
      anchored_message_text);
  cue->mutable_anchored_message_cue()->set_action_text(action_text);
  return response;
}

}  // namespace

class ContextualCueingTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kGeminiContextualSuggestionsCues,
          {{kGeminiContextualSuggestionsCuesOnDeviceClassifierParam, "true"},
           {kGeminiContextualSuggestionsCuesServerModelExecutionParam,
            "true"}}},
         {kPageActionMenu, {}}},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(GeminiServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeGeminiService));
    builder.AddTestingFactory(
        InProcessCategoryClassificationServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestInProcessClassificationService));
    builder.AddTestingFactory(
        OnDevicePageClassificationServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestPageClassificationService));
    builder.AddTestingFactory(
        ContextualCueingCapTrackerServiceFactory::GetInstance(),
        ContextualCueingCapTrackerServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeOptimizationGuideService));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    fake_opt_guide_service_ = static_cast<FakeOptimizationGuideService*>(
        OptimizationGuideServiceFactory::GetForProfile(profile_.get()));
    fake_page_classification_service_ =
        static_cast<FakeOnDevicePageClassificationService*>(
            OnDevicePageClassificationServiceFactory::GetForProfile(
                profile_.get()));

    auto* sync_service = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/true, syncer::UserSelectableTypeSet::All());

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "user@gmail.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    mutator.set_can_use_gemini_in_chrome(true);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->WasShown();
  }

  void TearDown() override {
    web_state_.reset();
    fake_page_classification_service_ = nullptr;
    fake_opt_guide_service_ = nullptr;
    profile_.reset();
    PlatformTest::TearDown();
  }

  void OnPageClassified(
      ContextualCueingTabHelper* tab_helper,
      const GURL& url,
      const std::vector<page_content_annotations::Category>& categories) {
    fake_page_classification_service_->SetCannedCategories(categories);
    tab_helper->PageLoaded(web_state_.get(),
                           web::PageLoadCompletionStatus::SUCCESS);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<FakeOptimizationGuideService> fake_opt_guide_service_ = nullptr;
  raw_ptr<FakeOnDevicePageClassificationService>
      fake_page_classification_service_ = nullptr;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that the tab helper can be created and retrieved.
TEST_F(ContextualCueingTabHelperTest, CreateAndRetrieve) {
  EXPECT_EQ(ContextualCueingTabHelper::FromWebState(web_state_.get()), nullptr);

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());

  EXPECT_NE(ContextualCueingTabHelper::FromWebState(web_state_.get()), nullptr);
}

// Tests that classification is ignored for off-the-record profiles.
TEST_F(ContextualCueingTabHelperTest, IgnoresOffTheRecordProfile) {
  ProfileIOS* otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories();
  auto otr_web_state = std::make_unique<web::FakeWebState>();
  otr_web_state->SetBrowserState(otr_profile);
  otr_web_state->SetCurrentURL(GURL("https://example.com"));

  ContextualCueingTabHelper::CreateForWebState(otr_web_state.get());
  auto* tab_helper =
      ContextualCueingTabHelper::FromWebState(otr_web_state.get());

  tab_helper->PageLoaded(otr_web_state.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
}

// Tests that classification is ignored for non-HTTP/HTTPS URLs.
TEST_F(ContextualCueingTabHelperTest, IgnoresNonHttpUrls) {
  web_state_->SetCurrentURL(GURL("chrome://version"));

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
}

// Tests that navigation resets classification state.
TEST_F(ContextualCueingTabHelperTest, NavigationResetsState) {
  web_state_->SetCurrentURL(GURL("https://example.com/page1"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/page2"));
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());

  tab_helper->RemoveObserver(&observer);
}

// Tests that hiding a tab cancels classification.
TEST_F(ContextualCueingTabHelperTest, WasHiddenCancelsClassification) {
  web_state_->SetCurrentURL(GURL("https://example.com"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->WasHidden(web_state_.get());
  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(fake_page_classification_service_->cancelled_web_state_ids_.size(),
            1u);
  EXPECT_EQ(fake_page_classification_service_->cancelled_web_state_ids_[0],
            web_state_->GetUniqueIdentifier());
}

// Tests successful model execution flow with a valid contextual cue response.
TEST_F(ContextualCueingTabHelperTest, ModelExecutionSuccessFlow) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_helper->GetContextualCue().has_value(); }));

  EXPECT_TRUE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(tab_helper->GetContextualCue()
                ->anchored_message_cue()
                .anchored_message_text(),
            "Buy now");
  EXPECT_EQ(
      tab_helper->GetContextualCue()->anchored_message_cue().action_text(),
      "Explore deals");
  EXPECT_EQ(observer.cue_call_count_, 1);

  tab_helper->RemoveObserver(&observer);
}

// Tests that classification with scores below the threshold does not trigger
// model execution.
TEST_F(ContextualCueingTabHelperTest,
       SubThresholdClassificationDoesNotCallMES) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.20f}};

  OnPageClassified(tab_helper, test_url, categories);

  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that history sync disabled prevents Model Execution Service request.
TEST_F(ContextualCueingTabHelperTest, HistorySyncOffBlocksMES) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  // Turn off history sync.
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.90f}};

  OnPageClassified(tab_helper, test_url, categories);

  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that age restricted users do not trigger Model Execution Service.
TEST_F(ContextualCueingTabHelperTest, AgeRestrictionBlocksMES) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  // Set user as age restricted.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kEducation, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);

  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that empty or invalid cues returned by MES are handled gracefully.
TEST_F(ContextualCueingTabHelperTest, EmptyCuesResponseHandledGracefully) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  // Response with no cues.
  optimization_guide::proto::ContextualCueingResponse response;
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);

  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that recording cue presentation, dismissal, and click works via
// TabHelper public API.
TEST_F(ContextualCueingTabHelperTest, RecordCueInteractions) {
  web_state_->SetCurrentURL(GURL("https://example.com/store/item1"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->RecordCueShown();
  tab_helper->RecordCueDismissed();
  tab_helper->RecordCueClicked();
}

// Tests that WasShown triggers classification for loaded pages if not yet
// classified.
TEST_F(ContextualCueingTabHelperTest, WasShownTriggersClassification) {
  web_state_->SetCurrentURL(GURL("https://example.com/item"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->WasShown(web_state_.get());
  EXPECT_EQ(fake_page_classification_service_->last_classified_web_state_id_,
            web_state_->GetUniqueIdentifier());
}

// Tests that navigation invalidates cues and notifies observers.
TEST_F(ContextualCueingTabHelperTest, NavigationNotifiesObserverInvalidation) {
  const GURL test_url("https://example.com/page1");
  web_state_->SetCurrentURL(test_url);

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_helper->GetContextualCue().has_value(); }));
  EXPECT_TRUE(tab_helper->GetContextualCue().has_value());

  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/page2"));
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  EXPECT_EQ(observer.invalidated_call_count_, 1);
  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());

  tab_helper->RemoveObserver(&observer);
}

// Tests that navigation without an active cue does not notify observers.
TEST_F(ContextualCueingTabHelperTest,
       NavigationWithoutCueDoesNotNotifyInvalidation) {
  web_state_->SetCurrentURL(GURL("https://example.com/page1"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/page2"));
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(false);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  EXPECT_EQ(observer.invalidated_call_count_, 0);
  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());

  tab_helper->RemoveObserver(&observer);
}

// Tests that same-document navigations trigger classification.
TEST_F(ContextualCueingTabHelperTest,
       SameDocumentNavigationTriggersClassification) {
  web_state_->SetCurrentURL(GURL("https://example.com/page1"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/page2"));
  nav_context.SetHasCommitted(true);
  nav_context.SetIsSameDocument(true);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);
}

// Tests that unsupported MIME types (like PDF) do not initiate classification
// or MES.
TEST_F(ContextualCueingTabHelperTest,
       UnsupportedMimeTypeDoesNotStartClassification) {
  const GURL test_url("https://example.com/document.pdf");
  web_state_->SetCurrentURL(test_url);
  web_state_->SetContentsMimeType("application/pdf");

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that background tabs provided by the delegate are attached to the MES
// request.
TEST_F(ContextualCueingTabHelperTest, BackgroundTabsCollectionFlow) {
  FakeContextualCueingDelegate delegate;
  delegate.SetBackgroundTabs({
      {GURL("https://example.com/bg1"), "Background Tab 1"},
      {GURL("https://example.com/bg2"), "Background Tab 2"},
  });

  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());
  tab_helper->SetDelegate(&delegate);

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_helper->GetContextualCue().has_value(); }));

  EXPECT_TRUE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 1);

  tab_helper->RemoveObserver(&observer);
}

// Tests that error page navigations do not count towards page spacing.
TEST_F(ContextualCueingTabHelperTest, ErrorPageIgnoredForPageSpacing) {
  web_state_->SetCurrentURL(GURL("https://example.com/page1"));
  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  web::FakeNavigationContext nav_context;
  nav_context.SetUrl(GURL("https://example.com/error"));
  nav_context.SetHasCommitted(true);
  nav_context.SetError([NSError errorWithDomain:NSURLErrorDomain
                                           code:NSURLErrorCannotConnectToHost
                                       userInfo:nil]);
  tab_helper->DidFinishNavigation(web_state_.get(), &nav_context);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
}

// Tests that responses without the expected Gemini in Chrome surface are
// rejected.
TEST_F(ContextualCueingTabHelperTest, MismatchedFulfillmentSurfaceRejected) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  // Response without gemini_in_chrome_surface.
  optimization_guide::proto::ContextualCueingResponse response;
  optimization_guide::proto::ContextualCue* cue =
      response.add_contextual_cues();
  cue->mutable_anchored_message_cue()->set_anchored_message_text("Buy now");
  cue->mutable_anchored_message_cue()->set_action_text("Explore deals");
  // Missing gemini_in_chrome_surface!

  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);

  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that history sync off blocks classification.
TEST_F(ContextualCueingTabHelperTest, HistorySyncOffBlocksClassification) {
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  // Turn off history sync.
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(observer.call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

// Tests that ineligible users do not initiate classification.
TEST_F(ContextualCueingTabHelperTest, IneligibleUserBlocksClassification) {
  base::HistogramTester histogram_tester;
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  // Set user as ineligible in GeminiService.
  auto* gemini_service = static_cast<FakeGeminiService*>(
      GeminiServiceFactory::GetForProfile(profile_.get()));
  gemini_service->SetIsEligible(false);

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(tab_helper->GetCategories().has_value());
  EXPECT_EQ(observer.call_count_, 0);
  histogram_tester.ExpectBucketCount(
      kContextualCueingDecisionHistogram,
      ContextualCueingDecision::kTargetFeatureNotEligible, 1);

  tab_helper->RemoveObserver(&observer);
}

// Tests that eligible profiles proceed past the Gemini eligibility gate.
TEST_F(ContextualCueingTabHelperTest, GeminiEligibilityAllowed) {
  base::HistogramTester histogram_tester;
  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  auto* gemini_service = static_cast<FakeGeminiService*>(
      GeminiServiceFactory::GetForProfile(profile_.get()));
  gemini_service->SetIsEligible(true);

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  tab_helper->PageLoaded(web_state_.get(),
                         web::PageLoadCompletionStatus::SUCCESS);

  histogram_tester.ExpectBucketCount(
      kContextualCueingDecisionHistogram,
      ContextualCueingDecision::kTargetFeatureNotEligible, 0);
}

// Tests that when model execution is disabled by feature flag, MES is not
// called even if classification succeeds.
TEST_F(ContextualCueingTabHelperTest,
       ModelExecutionDisabledByFlagDoesNotCallMES) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kGeminiContextualSuggestionsCues,
        {{kGeminiContextualSuggestionsCuesOnDeviceClassifierParam, "true"},
         {kGeminiContextualSuggestionsCuesServerModelExecutionParam, "false"}}},
       {kPageActionMenu, {}}},
      {});

  const GURL test_url("https://example.com/store/item123");
  web_state_->SetCurrentURL(test_url);

  auto response = CreateTestCueResponse("Buy now", "Explore deals");
  fake_opt_guide_service_->SetResponse(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, response,
      "optimization_guide.proto.ContextualCueingResponse");

  ContextualCueingTabHelper::CreateForWebState(web_state_.get());
  auto* tab_helper = ContextualCueingTabHelper::FromWebState(web_state_.get());

  TestCueingObserver observer;
  tab_helper->AddObserver(&observer);

  std::vector<page_content_annotations::Category> categories = {
      {page_content_annotations::CategoryType::kShopping, 0.85f}};

  OnPageClassified(tab_helper, test_url, categories);

  EXPECT_FALSE(tab_helper->GetContextualCue().has_value());
  EXPECT_EQ(observer.cue_call_count_, 0);

  tab_helper->RemoveObserver(&observer);
}

}  // namespace contextual_cueing
