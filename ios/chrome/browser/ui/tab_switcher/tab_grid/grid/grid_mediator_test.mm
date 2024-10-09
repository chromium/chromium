// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"

#import "base/containers/contains.h"
#import "base/test/ios/wait_util.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/model/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_delegate.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/test_scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Timeout for waiting for the TabCollectionConsumer updates.
constexpr base::TimeDelta kWaitForTabCollectionConsumerUpdateTimeout =
    base::Seconds(1);

// Name of the directory where snapshots are saved.
const char kIdentifier[] = "Identifier";

// List all ContentWorlds. Necessary because calling SetWebFramesManager(...)
// with a kAllContentWorlds is not enough with FakeWebState.
constexpr web::ContentWorld kContentWorlds[] = {
    web::ContentWorld::kAllContentWorlds,
    web::ContentWorld::kPageContentWorld,
    web::ContentWorld::kIsolatedWorld,
};

// Returns a `MockTabGroupSyncService`.
std::unique_ptr<KeyedService> CreateMockTabGroupSyncService(
    web::BrowserState* context) {
  return std::make_unique<tab_groups::MockTabGroupSyncService>();
}
}  // namespace

GridMediatorTestClass::GridMediatorTestClass() {
  InitializeFeatureFlags();
}

GridMediatorTestClass::~GridMediatorTestClass() = default;

void GridMediatorTestClass::SetUp() {
  PlatformTest::SetUp();

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                            FakeTabRestoreService::GetTestingFactory());
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateMockSyncService));
  builder.AddTestingFactory(AuthenticationServiceFactory::GetInstance(),
                            AuthenticationServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                            ios::HistoryServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(SessionRestorationServiceFactory::GetInstance(),
                            TestSessionRestorationService::GetTestingFactory());
  builder.AddTestingFactory(
      tab_groups::TabGroupSyncServiceFactory::GetInstance(),
      base::BindRepeating(&CreateMockTabGroupSyncService));
  profile_ = std::move(builder).Build();
  AuthenticationServiceFactory::CreateAndInitializeForProfile(
      profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
  // Price Drops are only available to signed in MSBB users.
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(identity);
  auth_service_ = static_cast<AuthenticationService*>(
      AuthenticationServiceFactory::GetInstance()->GetForProfile(
          profile_.get()));
  auth_service_->SignIn(identity,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  scene_state_ = OCMClassMock([SceneState class]);
  OCMStub([scene_state_ sceneSessionID]).andReturn(@(kIdentifier));
  browser_ = std::make_unique<TestBrowser>(
      profile_.get(), scene_state_,
      std::make_unique<BrowserWebStateListDelegate>());
  other_browser_ = std::make_unique<TestBrowser>(
      profile_.get(), nil, std::make_unique<BrowserWebStateListDelegate>());
  scene_loader_ = std::make_unique<TestSceneUrlLoadingService>();
  scene_loader_->current_browser_ = browser_.get();
  url_loading_delegate_ = [[FakeURLLoadingDelegate alloc] init];

  WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
  ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::FromBrowser(browser_.get())->SetSessionID(kIdentifier);

  // Create loaders, insertion and notifier agents.
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
  UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
  TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
  loader_ = UrlLoadingBrowserAgent::FromBrowser(browser_.get());
  loader_->SetSceneService(scene_loader_.get());
  loader_->SetDelegate(url_loading_delegate_);

  SessionRestorationServiceFactory::GetForProfile(profile_.get())
      ->SetSessionID(browser_.get(), kIdentifier);
  browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
  browser_list_->AddBrowser(browser_.get());
  browser_list_->AddBrowser(other_browser_.get());

  // Insert some web states.
  std::vector<std::string> urls{"https://foo/bar", "https://car/tar",
                                "https://hello/world"};
  std::vector<web::WebStateID> identifiers;
  for (int i = 0; i < 3; i++) {
    auto web_state = CreateFakeWebStateWithURL(GURL(urls[i]));
    web::WebStateID identifier = web_state.get()->GetUniqueIdentifier();
    // Tab IDs should be unique.
    ASSERT_FALSE(base::Contains(identifiers, identifier));
    identifiers.push_back(identifier);
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state), WebStateList::InsertionParams::AtIndex(i));
  }
  original_identifiers_ = identifiers;
  browser_->GetWebStateList()->ActivateWebStateAt(1);
  original_selected_identifier_ =
      browser_->GetWebStateList()->GetWebStateAt(1)->GetUniqueIdentifier();
  consumer_ = [[FakeTabCollectionConsumer alloc] init];
  fake_toolbars_mediator_ = [[FakeTabGridToolbarsMediator alloc] init];
}

std::unique_ptr<web::FakeWebState>
GridMediatorTestClass::CreateFakeWebStateWithURL(const GURL& url) {
  auto web_state = std::make_unique<web::FakeWebState>();
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
  navigation_manager->SetLastCommittedItem(
      navigation_manager->GetItemAtIndex(0));
  web_state->SetNavigationManager(std::move(navigation_manager));
  for (const web::ContentWorld content_world : kContentWorlds) {
    web_state->SetWebFramesManager(
        content_world, std::make_unique<web::FakeWebFramesManager>());
  }
  web_state->SetBrowserState(profile_.get());
  web_state->SetNavigationItemCount(1);
  web_state->SetCurrentURL(url);
  return web_state;
}

void GridMediatorTestClass::TearDown() {
  PlatformTest::TearDown();
  SessionRestorationServiceFactory::GetForProfile(profile_.get())
      ->Disconnect(browser_.get());
}

bool GridMediatorTestClass::WaitForConsumerUpdates(size_t expected_count) {
  return WaitUntilConditionOrTimeout(
      kWaitForTabCollectionConsumerUpdateTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return expected_count == consumer_.items.size();
      });
}

void GridMediatorTestClass::InitializeFeatureFlags() {}
