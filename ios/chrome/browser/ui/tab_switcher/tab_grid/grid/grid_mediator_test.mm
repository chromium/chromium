// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"

#import "base/containers/contains.h"
#import "base/test/ios/wait_util.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/sessions/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Timeout for waiting for the TabCollectionConsumer updates.
constexpr base::TimeDelta kWaitForTabCollectionConsumerUpdateTimeout =
    base::Seconds(1);

std::unique_ptr<KeyedService> BuildFakeTabRestoreService(
    web::BrowserState* browser_state) {
  return std::make_unique<FakeTabRestoreService>();
}
}  // namespace

// Fake WebStateList delegate that attaches the required tab helper.
class TabHelperFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabHelperFakeWebStateListDelegate() {}
  ~TabHelperFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
    id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
    NewTabPageTabHelper::CreateForWebState(web_state);
    NewTabPageTabHelper::FromWebState(web_state)->SetDelegate(delegate);
    PagePlaceholderTabHelper::CreateForWebState(web_state);
    SnapshotTabHelper::CreateForWebState(web_state);
    if (web::UseNativeSessionRestorationCache()) {
      WebSessionStateTabHelper::CreateForWebState(web_state);
    }
  }
};

GridMediatorTestClass::GridMediatorTestClass() {
  InitializeFeatureFlags();
}

GridMediatorTestClass::~GridMediatorTestClass() = default;

void GridMediatorTestClass::SetUp() {
  PlatformTest::SetUp();

  TestChromeBrowserState::Builder builder;
  builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                            base::BindRepeating(BuildFakeTabRestoreService));
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateMockSyncService));
  builder.AddTestingFactory(AuthenticationServiceFactory::GetInstance(),
                            AuthenticationServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                            ios::HistoryServiceFactory::GetDefaultFactory());
  browser_state_ = builder.Build();
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      browser_state_.get(),
      std::make_unique<FakeAuthenticationServiceDelegate>());
  // Price Drops are only available to signed in MSBB users.
  browser_state_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(identity);
  auth_service_ = static_cast<AuthenticationService*>(
      AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
          browser_state_.get()));
  auth_service_->SignIn(identity,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  browser_ = std::make_unique<TestBrowser>(
      browser_state_.get(),
      std::make_unique<TabHelperFakeWebStateListDelegate>());
  WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
  ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::FromBrowser(browser_.get())
      ->SetSessionID([[NSUUID UUID] UUIDString]);
  browser_list_ = BrowserListFactory::GetForBrowserState(browser_state_.get());
  browser_list_->AddBrowser(browser_.get());

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
        i, std::move(web_state), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener());
  }
  original_identifiers_ = identifiers;
  browser_->GetWebStateList()->ActivateWebStateAt(1);
  original_selected_identifier_ =
      browser_->GetWebStateList()->GetWebStateAt(1)->GetUniqueIdentifier();
  consumer_ = [[FakeTabCollectionConsumer alloc] init];
}

std::unique_ptr<web::FakeWebState>
GridMediatorTestClass::CreateFakeWebStateWithURL(const GURL& url) {
  auto web_state = std::make_unique<web::FakeWebState>();
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
  navigation_manager->SetLastCommittedItem(
      navigation_manager->GetItemAtIndex(0));
  web_state->SetNavigationManager(std::move(navigation_manager));
  web_state->SetWebFramesManager(std::make_unique<web::FakeWebFramesManager>());
  web_state->SetBrowserState(browser_state_.get());
  web_state->SetNavigationItemCount(1);
  web_state->SetCurrentURL(url);
  SnapshotTabHelper::CreateForWebState(web_state.get());
  return web_state;
}

void GridMediatorTestClass::TearDown() {
  PlatformTest::TearDown();
}

void GridMediatorTestClass::SetFakePriceDrop(web::WebState* web_state) {
  auto price_drop =
      std::make_unique<ShoppingPersistedDataTabHelper::PriceDrop>();
  price_drop->current_price = @"$5";
  price_drop->previous_price = @"$10";
  price_drop->url = web_state->GetLastCommittedURL();
  price_drop->timestamp = base::Time::Now();
  ShoppingPersistedDataTabHelper::FromWebState(web_state)
      ->SetPriceDropForTesting(std::move(price_drop));
}

bool GridMediatorTestClass::WaitForConsumerUpdates(size_t expected_count) {
  return WaitUntilConditionOrTimeout(
      kWaitForTabCollectionConsumerUpdateTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return expected_count == consumer_.items.size();
      });
}

void GridMediatorTestClass::InitializeFeatureFlags() {
  scoped_feature_list_.InitAndDisableFeature(
      web::features::kEnableSessionSerializationOptimizations);
}
