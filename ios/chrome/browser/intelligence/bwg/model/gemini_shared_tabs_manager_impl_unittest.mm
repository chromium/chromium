// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_shared_tabs_manager_impl.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <set>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace ios::provider {
int GetUpdateActivePageContextCallCount();
void ResetGemini();
}  // namespace ios::provider

// Test fixture for `GeminiSharedTabsManagerImpl`.
class GeminiSharedTabsManagerImplTest : public PlatformTest {
 protected:
  GeminiSharedTabsManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        {kPageActionMenu, kPageContextExtractorRefactored}, {});
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    personal_profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(profile_builder));

    gemini::test::SetUpEligibleAccount(personal_profile_);
    browser_ = std::make_unique<TestBrowser>(personal_profile_);
    PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
    shared_tabs_manager_ = std::make_unique<GeminiSharedTabsManagerImpl>(
        browser_->GetWebStateList(), personal_profile_->GetPrefs(),
        PersistTabContextBrowserAgent::FromBrowser(browser_.get()),
        SnapshotBrowserAgent::FromBrowser(browser_.get()),
        base::BindRepeating(&GeminiSharedTabsManagerImplTest::IsFloatyInvoked,
                            base::Unretained(this)),
        base::BindRepeating([]() { return false; }));

    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state->SetBrowserState(personal_profile_);
    web_state->SetCurrentURL(GURL("chrome://newtab/"));
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    GeminiTabHelper::CreateForWebState(web_state.get());
    WebViewProxyTabHelper::CreateForWebState(web_state.get());

    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForProfile(
            personal_profile_, ServiceAccessType::IMPLICIT_ACCESS));

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
  }

  void TearDown() override {
    shared_tabs_manager_->Shutdown();
    shared_tabs_manager_.reset();
    web_state_ = nullptr;
    personal_profile_ = nullptr;
    browser_.reset();
    profile_manager_.PrepareForDestruction();
    ios::provider::ResetGemini();
    PlatformTest::TearDown();
  }

  // Returns whether the floaty is currently invoked in tests.
  bool IsFloatyInvoked() const { return is_floaty_invoked_; }

  // Sets whether the floaty is currently invoked in tests.
  void SetIsFloatyInvoked(bool is_invoked) { is_floaty_invoked_ = is_invoked; }

  // Triggers `OnPersistTabContextLookupComplete()` in the shared tabs manager.
  void TriggerPersistTabContextLookupComplete(
      PersistTabContextBrowserAgent::PageContextMap contexts_map) {
    shared_tabs_manager_->OnPersistTabContextLookupComplete(
        std::move(contexts_map));
  }

  // Getter for raw `shared_tabs_` member.
  const GeminiSharedTabsManagerImpl::SharedTabsList& GetRawSharedTabs() const {
    return shared_tabs_manager_->shared_tabs_;
  }

  // Getter for an attached tab context by ID.
  GeminiPageContext* GetRawAttachedTabContext(web::WebStateID id) const {
    return shared_tabs_manager_->GetSharedPageContext(id);
  }

  // Setter for raw `shared_tabs_` member.
  void SetRawAttachedTab(web::WebStateID id, GeminiPageContext* page_context) {
    shared_tabs_manager_->SetSharedPageContext(id, page_context);
  }

  // Wrapper for `SharedTabsCount`.
  NSUInteger SharedTabsCount() {
    return shared_tabs_manager_->SharedTabsCount();
  }

  // Wrapper for `GetInactiveSharedTabs`.
  NSUInteger GetInactiveSharedTabsCount() {
    return shared_tabs_manager_->GetInactiveSharedTabs().count;
  }

  // Wrapper for `DetachTabWithID`.
  void DetachTabWithID(NSString* tab_id) {
    shared_tabs_manager_->DetachTabWithID(tab_id);
  }

  // Wrapper for `UpdateLocalTabAttachmentState`.
  void UpdateLocalTabAttachmentState(
      NSString* tab_id,
      ios::provider::GeminiPageContextAttachmentState new_state) {
    shared_tabs_manager_->UpdateLocalTabAttachmentState(tab_id, new_state);
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> personal_profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  bool is_floaty_invoked_ = false;
  std::unique_ptr<GeminiSharedTabsManagerImpl> shared_tabs_manager_;
};

// Tests that `DetachTabWithID` gracefully handles an invalid tab ID string.
TEST_F(GeminiSharedTabsManagerImplTest, TestDetachInvalidTabId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id});
  size_t initial_size = GetRawSharedTabs().size();

  DetachTabWithID(@"invalid_id");

  // The map size should be unchanged.
  EXPECT_EQ(initial_size, GetRawSharedTabs().size());
}

// Tests that `UpdateLocalTabAttachmentState` updates the attachment state of
// the active tab without removing it.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestUpdateLocalTabAttachmentStateDetachesActiveTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  GeminiPageContext* mock_context = [[GeminiPageContext alloc] init];
  mock_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, mock_context);

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id});

  // Verify it starts as attached.
  auto tabs = GetRawSharedTabs();
  ASSERT_EQ(1u, tabs.size());
  ASSERT_EQ(
      ios::provider::GeminiPageContextAttachmentState::kAttached,
      GetRawAttachedTabContext(active_id).geminiPageContextAttachmentState);

  NSString* tab_id_str =
      [NSString stringWithFormat:@"%d", active_id.identifier()];
  base::UserActionTester user_action_tester;
  UpdateLocalTabAttachmentState(
      tab_id_str, ios::provider::GeminiPageContextAttachmentState::kDetached);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("MobileGeminiActiveTabDetached"));

  UpdateLocalTabAttachmentState(
      tab_id_str, ios::provider::GeminiPageContextAttachmentState::kAttached);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("MobileGeminiActiveTabAttached"));

  // Verify it is in the map as attached.
  tabs = GetRawSharedTabs();
  EXPECT_EQ(1u, tabs.size());
  EXPECT_EQ(
      ios::provider::GeminiPageContextAttachmentState::kAttached,
      GetRawAttachedTabContext(active_id).geminiPageContextAttachmentState);
}

// Tests that `DetachTabWithID` completely removes a shared tab from the cache.
TEST_F(GeminiSharedTabsManagerImplTest, TestDetachSharedTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});

  auto tabs = GetRawSharedTabs();
  ASSERT_EQ(2u, tabs.size());

  NSString* other_tab_id_str =
      [NSString stringWithFormat:@"%d", other_id.identifier()];
  base::UserActionTester user_action_tester;
  DetachTabWithID(other_tab_id_str);
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileGeminiTabDetached"));

  // Verify the shared tab is completely removed.
  tabs = GetRawSharedTabs();
  EXPECT_EQ(1u, tabs.size());
  EXPECT_EQ(nil, GetRawAttachedTabContext(other_id));
}

// Tests that disabling the page content sharing pref clears attached tabs.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestClearSharedTabsOnPageContentPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  // Add the active tab.
  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  // Add a shared tab.
  web::WebStateID other_id = web::WebStateID::NewUnique();
  GeminiPageContext* other_context = [[GeminiPageContext alloc] init];
  other_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(other_id, other_context);

  EXPECT_EQ(2u, GetRawSharedTabs().size());

  // Toggle the preference to disabled.
  personal_profile_->GetPrefs()->SetBoolean(prefs::kIOSBWGPageContentSetting,
                                            false);

  // Verify that `shared_tabs_` was cleared.
  EXPECT_EQ(0u, GetRawSharedTabs().size());
}

// Tests that `OnTabPickerSelectionChanged` correctly assigns partial contexts
// to newly selected tabs and triggers a fetch.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestOnTabPickerSelectionChangedNewlyAddedTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  // Add the active tab.
  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  // Add a newly selected tab.
  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});

  // Verify that `shared_tabs_` now contains a partial context for the new
  // tab.
  auto tabs = GetRawSharedTabs();
  ASSERT_EQ(2u, tabs.size());

  GeminiPageContext* other_context = GetRawAttachedTabContext(other_id);
  ASSERT_TRUE(other_context);
  EXPECT_EQ(ios::provider::GeminiPageContextAttachmentState::kAttached,
            other_context.geminiPageContextAttachmentState);
  // `FakeWebState` lacks a URL and MIME type, making it ineligible for Gemini
  // context extraction. We leave it ineligible so that it falls back to
  // `kBlocked`, avoiding actual native view snapshot logic which crashes tests.
  EXPECT_EQ(ios::provider::GeminiPageContextComputationState::kBlocked,
            other_context.geminiPageContextComputationState);
}

// Tests that `OnPersistTabContextLookupComplete` delegates to
// `GeneratePageContext`
// when cached APC does not exist (empty optional).
TEST_F(GeminiSharedTabsManagerImplTest,
       TestOnPersistTabContextLookupCompleteCachedAPCDoesNotExist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});

  // Fire callback indicating cached APC does not exist.
  PersistTabContextBrowserAgent::PageContextMap cache_map;
  cache_map[base::NumberToString(other_id.identifier())] = std::nullopt;
  TriggerPersistTabContextLookupComplete(std::move(cache_map));

  // Since cached APC does not exist, `GeneratePageContext` is triggered.
  // However, because `FakeWebState` is naturally ineligible (no URL or MIME
  // type), it falls back to `kBlocked` without attempting to take a native view
  // snapshot
  // (which would crash the test runner).
  GeminiPageContext* other_context = GetRawAttachedTabContext(other_id);
  EXPECT_EQ(ios::provider::GeminiPageContextComputationState::kBlocked,
            other_context.geminiPageContextComputationState);
}

// Tests that `OnPersistTabContextLookupComplete` correctly upgrades the context
// to `kSuccess` when cached APC exists.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestOnPersistTabContextLookupCompleteCachedAPCExists) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});

  // Fire callback indicating cached APC exists.
  PersistTabContextBrowserAgent::PageContextMap cache_map;
  cache_map[base::NumberToString(other_id.identifier())] =
      std::make_unique<optimization_guide::proto::PageContext>();
  TriggerPersistTabContextLookupComplete(std::move(cache_map));

  // Since cached APC exists, the page context should be updated to `kSuccess`.
  GeminiPageContext* other_context = GetRawAttachedTabContext(other_id);
  EXPECT_EQ(ios::provider::GeminiPageContextComputationState::kSuccess,
            other_context.geminiPageContextComputationState);
  EXPECT_TRUE(other_context.uniquePageContext != nullptr);
}

// Tests that a shared tab's page context fetched while the floaty is invoked is
// sent to the provider.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestSharedTabPageContextSentToProviderWhileFloatyInvoked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  SetIsFloatyInvoked(true);
  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});
  ios::provider::ResetGemini();

  PersistTabContextBrowserAgent::PageContextMap cache_map;
  cache_map[base::NumberToString(other_id.identifier())] =
      std::make_unique<optimization_guide::proto::PageContext>();
  TriggerPersistTabContextLookupComplete(std::move(cache_map));

  EXPECT_EQ(1, ios::provider::GetUpdateActivePageContextCallCount());
}

// Tests that a shared tab's page context fetched after the floaty was dismissed
// is kept for the next invocation without being sent to the provider.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestSharedTabPageContextNotSentToProviderAfterFloatyDismissed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  SetIsFloatyInvoked(true);
  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});
  // Dismissing the floaty resets the provider, including its recorded calls.
  SetIsFloatyInvoked(false);
  ios::provider::ResetGemini();

  PersistTabContextBrowserAgent::PageContextMap cache_map;
  cache_map[base::NumberToString(other_id.identifier())] =
      std::make_unique<optimization_guide::proto::PageContext>();
  TriggerPersistTabContextLookupComplete(std::move(cache_map));

  GeminiPageContext* other_context = GetRawAttachedTabContext(other_id);
  ASSERT_TRUE(other_context);
  EXPECT_EQ(ios::provider::GeminiPageContextComputationState::kSuccess,
            other_context.geminiPageContextComputationState);
  EXPECT_EQ(0, ios::provider::GetUpdateActivePageContextCallCount());
}

// Tests that switching away from a shared tab immediately triggers a page
// context refetch for that tab.
TEST_F(GeminiSharedTabsManagerImplTest, TestSwitchingTabRefetchesSharedTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});

  // Switch to `other_id` and manually set its computation state to `kSuccess`.
  browser_->GetWebStateList()->ActivateWebStateAt(1);
  GetRawAttachedTabContext(other_id).geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kSuccess;

  // Switch back to `active_id` so that we switch away from `other_id`, which
  // should immediately trigger a refetch for `other_id`.
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  // Verify that a fresh partial context was generated for `other_id`. Since
  // `FakeWebState` lacks a URL, it is ineligible and the refetch immediately
  // sets its partial context state to `kBlocked`.
  EXPECT_EQ(
      ios::provider::GeminiPageContextComputationState::kBlocked,
      GetRawAttachedTabContext(other_id).geminiPageContextComputationState);
}

// Tests that closing a shared tab in `WebStateList` removes it from
// `shared_tabs_`.
TEST_F(GeminiSharedTabsManagerImplTest, TestClosingWebStateRemovesSharedTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});
  ASSERT_EQ(2u, GetRawSharedTabs().size());

  browser_->GetWebStateList()->CloseWebStateAt(
      1, WebStateList::ClosingReason::kUserAction);

  EXPECT_EQ(1u, GetRawSharedTabs().size());
  EXPECT_EQ(nil, GetRawAttachedTabContext(other_id));
}

// Tests that the metrics block providers report accurate shared tab counts.
TEST_F(GeminiSharedTabsManagerImplTest, TestMetricsBlockProviders) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  // Add the active tab.
  web::WebStateID active_id = web_state_->GetUniqueIdentifier();
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  EXPECT_EQ(1u, SharedTabsCount());
  EXPECT_FALSE(GetInactiveSharedTabsCount() > 0);

  // Add a shared tab.
  web::WebStateID other_id = web::WebStateID::NewUnique();
  GeminiPageContext* other_context = [[GeminiPageContext alloc] init];
  other_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(other_id, other_context);

  EXPECT_EQ(2u, SharedTabsCount());
  EXPECT_TRUE(GetInactiveSharedTabsCount() > 0);

  // Set active tab to detached.
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kDetached;
  EXPECT_EQ(1u, SharedTabsCount());
  EXPECT_TRUE(GetInactiveSharedTabsCount() > 0);
}

// Tests that attached inactive shared tabs preserve their insertion order
// regardless of `WebStateID` values or subsequent context updates.
TEST_F(GeminiSharedTabsManagerImplTest,
       TestInactiveSharedTabsPreserveInsertionOrder) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  // Create `WebStateID`s out of numerical order to verify that sorting is by
  // insertion order rather than by `WebStateID` value.
  web::WebStateID id_first = web::WebStateID::FromSerializedValue(100);
  web::WebStateID id_second = web::WebStateID::FromSerializedValue(50);
  web::WebStateID id_third = web::WebStateID::FromSerializedValue(200);

  GeminiPageContext* context_first = [[GeminiPageContext alloc] init];
  context_first.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(id_first, context_first);

  GeminiPageContext* context_second = [[GeminiPageContext alloc] init];
  context_second.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(id_second, context_second);

  GeminiPageContext* context_third = [[GeminiPageContext alloc] init];
  context_third.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(id_third, context_third);

  auto raw_tabs = GetRawSharedTabs();
  ASSERT_EQ(3u, raw_tabs.size());
  EXPECT_EQ(id_first, raw_tabs[0].first);
  EXPECT_EQ(id_second, raw_tabs[1].first);
  EXPECT_EQ(id_third, raw_tabs[2].first);

  // Updating an existing tab's context should not alter insertion order.
  GeminiPageContext* updated_context_first = [[GeminiPageContext alloc] init];
  updated_context_first.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(id_first, updated_context_first);

  raw_tabs = GetRawSharedTabs();
  ASSERT_EQ(3u, raw_tabs.size());
  EXPECT_EQ(id_first, raw_tabs[0].first);
  EXPECT_EQ(id_second, raw_tabs[1].first);
  EXPECT_EQ(id_third, raw_tabs[2].first);
  EXPECT_EQ(updated_context_first, raw_tabs[0].second);
}

// Tests that `UpdateSharedTabsForActiveWebState` persists the shared-tab
// selection when the active tab is attached and clears it when switching to a
// non-selected tab.
TEST_F(GeminiSharedTabsManagerImplTest, TestUpdateSharedTabsForActiveWebState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  std::unique_ptr<web::FakeWebState> non_selected_web_state =
      std::make_unique<web::FakeWebState>();
  non_selected_web_state->SetBrowserState(personal_profile_);
  GeminiTabHelper::CreateForWebState(non_selected_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(non_selected_web_state.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(non_selected_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  shared_tabs_manager_->SaveActivePageContextToSharedTabs(active_context);

  shared_tabs_manager_->OnTabPickerSelectionChanged({active_id, other_id});
  EXPECT_EQ(2u, SharedTabsCount());
  EXPECT_EQ(1u, GetInactiveSharedTabsCount());

  // Switching to an attached tab preserves the selection.
  browser_->GetWebStateList()->ActivateWebStateAt(1);
  shared_tabs_manager_->UpdateSharedTabsForActiveWebState(
      browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_EQ(2u, SharedTabsCount());
  EXPECT_EQ(1u, GetInactiveSharedTabsCount());

  // Switching to a non-selected tab clears the selection.
  browser_->GetWebStateList()->ActivateWebStateAt(2);
  shared_tabs_manager_->UpdateSharedTabsForActiveWebState(
      browser_->GetWebStateList()->GetActiveWebState());
  EXPECT_EQ(0u, GetInactiveSharedTabsCount());
  EXPECT_EQ(0u, SharedTabsCount());
  EXPECT_TRUE(GetRawSharedTabs().empty());
}
