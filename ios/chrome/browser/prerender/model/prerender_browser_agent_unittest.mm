// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"

#import "base/check_deref.h"
#import "base/memory/raw_ptr.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/mock_network_change_notifier.h"
#import "net/base/network_change_notifier.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// List all content world for which a TestWebFramesManager should be installed.
static const web::ContentWorld kAllContentWorlds[] = {
    web::ContentWorld::kAllContentWorlds,
    web::ContentWorld::kPageContentWorld,
    web::ContentWorld::kIsolatedWorld,
};

// Test double implementation of NavigationManager.
class TestNavigationManager : public web::FakeNavigationManager {
 public:
  explicit TestNavigationManager(web::FakeWebState* web_state)
      : web::FakeNavigationManager(), web_state_(CHECK_DEREF(web_state)) {
    SetBrowserState(web_state_->GetBrowserState());
  }

  // web::NavigationManager implementation.
  void LoadURLWithParams(const WebLoadParams& load_params) override {
    web::FakeNavigationManager::LoadURLWithParams(load_params);
    web_state_->SetVisibleURL(load_params.url);
  }

 private:
  const raw_ref<web::FakeWebState> web_state_;
};

// Test double implementation of WebState.
class TestWebState : public web::FakeWebState {
 public:
  explicit TestWebState(ProfileIOS* profile)
      : FakeWebState(web::WebStateID::NewUnique()) {
    SetBrowserState(profile);
    SetNavigationManager(std::make_unique<TestNavigationManager>(this));

    SetWebFramesManager(std::make_unique<web::FakeWebFramesManager>());
    for (auto content_world : kAllContentWorlds) {
      SetWebFramesManager(content_world,
                          std::make_unique<web::FakeWebFramesManager>());
    }
  }

  // web::WebState implementation.
  std::unique_ptr<web::WebState> Clone() const override {
    return std::make_unique<TestWebState>(
        ProfileIOS::FromBrowserState(GetBrowserState()));
  }
};

// NetworkChangeObserver that invoke a callback when the connection type
// changes.
class ScopedNetworkChangeObserver
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  explicit ScopedNetworkChangeObserver(base::RepeatingClosure closure)
      : closure_(std::move(closure)) {
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  ~ScopedNetworkChangeObserver() override {
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    closure_.Run();
  }

 private:
  base::RepeatingClosure closure_;
};

// Converts NetworkPredictionSetting to a string.
std::string_view ToString(prerender_prefs::NetworkPredictionSetting value) {
  switch (value) {
    case prerender_prefs::NetworkPredictionSetting::kDisabled:
      return "Disabled";

    case prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly:
      return "WiFi only";

    case prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular:
      return "Always";
  }
}

// Converts net::NetworkChangeNotifier::ConnectionType to a string.
std::string_view ToString(net::NetworkChangeNotifier::ConnectionType value) {
  switch (value) {
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI:
      return "WiFi";

    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G:
      return "Cellular";

    default:
      return "Unknown";
  }
}

}  // namespace

class PrerenderBrowserAgentTest : public PlatformTest {
 public:
  PrerenderBrowserAgentTest() {
    // Setup a mock NetworkChangeNotifier to simulate different network
    // connection types.
    network_change_notifier_ = net::test::MockNetworkChangeNotifier::Create();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    PrerenderBrowserAgent::CreateForBrowser(browser_.get());

    // Prerendering clones the active WebState from the Browser's WebStateList
    // or fail if there is none. Insert a WebState to avoid this failure state.
    browser_->GetWebStateList()->InsertWebState(
        std::make_unique<TestWebState>(profile_.get()),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  // Returns the PrerenderBrowserAgent instance.
  PrerenderBrowserAgent* agent() {
    return PrerenderBrowserAgent::FromBrowser(browser_.get());
  }

  // Returns the WebStateList.
  WebStateList* web_state_list() { return browser_->GetWebStateList(); }

  // Returns the ProfileIOS.
  ProfileIOS* profile() { return profile_.get(); }

  // Helper that start prerender and wait until the PrerenderBrowserAgent
  // has processed the request (even if the policy is kNoDelay, the request
  // is only processed on the next run of the RunLoop).
  void StartPrerender(const GURL& url, ui::PageTransition transition) {
    agent()->StartPrerender(url, web::Referrer(), transition,
                            PrerenderBrowserAgent::PrerenderPolicy::kNoDelay);

    // Give some time to PrerenderBrowserAgent to start the prerendering.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::TimeDelta());

    run_loop.Run();
  }

  // Set "NetworkPredictionSetting" preference.
  void SetNetworkPredictionSetting(
      prerender_prefs::NetworkPredictionSetting value) {
    profile_->GetPrefs()->SetInteger(prefs::kNetworkPredictionSetting,
                                     base::to_underlying(value));
  }

  // Set "NetworkPredictionSetting" to "Enabled on WiFi & Cellular"
  void PrerenderEnabledOnWiFiAndCellular() {
    SetNetworkPredictionSetting(
        prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular);
  }

  // Set "NetworkPredictionSetting" to "Enabled on WiFi Only"
  void PrerenderEnabledOnWiFiOnly() {
    SetNetworkPredictionSetting(
        prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly);
  }

  // Set "NetworkPredictionSetting" to "Disabled"
  void PrerenderDisabled() {
    SetNetworkPredictionSetting(
        prerender_prefs::NetworkPredictionSetting::kDisabled);
  }

  // Simulate network connection type.
  void SimulateNetworkConnectionType(
      net::NetworkChangeNotifier::ConnectionType type) {
    if (network_change_notifier_->GetCurrentConnectionType() == type) {
      return;
    }

    base::RunLoop run_loop;
    ScopedNetworkChangeObserver observer(run_loop.QuitClosure());
    network_change_notifier_->SetConnectionTypeAndNotifyObservers(type);
    run_loop.Run();
  }

  // Simulate a "WiFi" network connection.
  void SimulateWiFiConnection() {
    SimulateNetworkConnectionType(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  }

  // Simulate a "Cellular" network connection.
  void SimulateCellularConnection() {
    SimulateNetworkConnectionType(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G);
  }

  // Simulate an offline network connection.
  void SimulateOffline() {
    SimulateNetworkConnectionType(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;

  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Check that StartPrerender works as expect.
TEST_F(PrerenderBrowserAgentTest, StartPrerender) {
  constexpr struct TestCase {
    std::string_view url;
    prerender_prefs::NetworkPredictionSetting pref;
    net::NetworkChangeNotifier::ConnectionType type;
    bool expected;
  } kTestCases[] = {
      // HTTPS, Prerender enabled on WiFi & Cellular.
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
          true,
      },
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G,
          true,
      },
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
          false,
      },
      // HTTPS, Prerender enabled on WiFi Only.
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
          true,
      },
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G,
          false,
      },
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
          false,
      },
      // HTTPS, Prerender disabled.
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kDisabled,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
          false,
      },
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kDisabled,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G,
          false,
      },
      {
          "https://www.google.com/",
          prerender_prefs::NetworkPredictionSetting::kDisabled,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
          false,
      },
      // Do not pre-render non-HTTP & non-HTTPS URLs.
      {
          "",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
          false,
      },
      {
          "chrome://newtab",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
          false,
      },
      {
          "about:flags",
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiAndCellular,
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
          false,
      },
  };

  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;
  for (const auto& test_case : kTestCases) {
    SetNetworkPredictionSetting(test_case.pref);
    SimulateNetworkConnectionType(test_case.type);

    // Replace the active WebState with a new WebState (since prerender
    // is not enabled if the current WebState is already displaying the
    // requested url).
    WebStateList* list = web_state_list();
    const int active_index = list->active_index();
    list->ReplaceWebStateAt(active_index,
                            std::make_unique<TestWebState>(profile()));

    StartPrerender(GURL(test_case.url), transition);
    EXPECT_EQ(test_case.expected,
              agent()->ValidatePrerender(GURL(test_case.url), transition))
        << "    Where url = \"" << test_case.url << "\", pref = \""
        << ToString(test_case.pref) << "\", connection = \""
        << ToString(test_case.type) << "\"";
  }
}

// Check that prerender is cancelled if enabled only on WiFi and the
// connection type changes from WiFi to Cellular.
TEST_F(PrerenderBrowserAgentTest, Cancel_ConnectionWiFiToCellular) {
  PrerenderEnabledOnWiFiOnly();
  SimulateWiFiConnection();

  const GURL url("https://www.google.com/");
  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;
  StartPrerender(url, transition);

  // Change connection from WiFi to Cellular. This should cancel prerender.
  SimulateCellularConnection();
  EXPECT_FALSE(agent()->ValidatePrerender(url, transition));
}

// Check that prerender is cancelled if enabled only on WiFi and the
// connection type changes from WiFi to no connection.
TEST_F(PrerenderBrowserAgentTest, Cancel_ConnectionWiFiToOffline) {
  PrerenderEnabledOnWiFiOnly();
  SimulateWiFiConnection();

  const GURL url("https://www.google.com/");
  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;
  StartPrerender(url, transition);

  // Change connection from WiFi to none. This should cancel prerender.
  SimulateOffline();
  EXPECT_FALSE(agent()->ValidatePrerender(url, transition));
}

// Check that prerender is cancelled if the connection is ceullular and the
// connection settings is changed from "WiFi & Cellular" to "WiFi only".
TEST_F(PrerenderBrowserAgentTest, Cancel_EnabledWifiAndCellularToWiFiOnly) {
  PrerenderEnabledOnWiFiAndCellular();
  SimulateCellularConnection();

  const GURL url("https://www.google.com/");
  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;
  StartPrerender(url, transition);

  // Change setting from "Wifi & Cellular" to "WiFi Only". This should cancel
  // the prerender.
  PrerenderEnabledOnWiFiOnly();
  EXPECT_FALSE(agent()->ValidatePrerender(url, transition));
}

// Check that prerender is cancelled if the connection is ceullular and the
// connection settings is changed from "WiFi & Cellular" to "Never".
TEST_F(PrerenderBrowserAgentTest, Cancel_EnabledWifiAndCellularToDisabled) {
  PrerenderEnabledOnWiFiAndCellular();
  SimulateCellularConnection();

  const GURL url("https://www.google.com/");
  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;
  StartPrerender(url, transition);

  // Change setting from "Wifi & Cellular" to "Never". This should cancel
  // the prerender.
  PrerenderDisabled();
  EXPECT_FALSE(agent()->ValidatePrerender(url, transition));
}

// Check that prerender is disabled if the WebStateList has no active WebState.
TEST_F(PrerenderBrowserAgentTest, DisabledIfNoActiveWebState) {
  WebStateList* list = web_state_list();
  CloseAllWebStates(CHECK_DEREF(list), WebStateList::ClosingReason::kDefault);
  ASSERT_TRUE(list->empty());

  PrerenderEnabledOnWiFiAndCellular();
  SimulateWiFiConnection();

  const GURL url("https://www.google.com/");
  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;

  StartPrerender(url, transition);
  EXPECT_FALSE(agent()->ValidatePrerender(url, transition));
}

// Check that prerender is disabled for supervised users.
TEST_F(PrerenderBrowserAgentTest, DisabledForSupervisedUsers) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@gmail.com", signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account, identity_manager, /*is_subject_to_parental_controls=*/true);

  PrerenderEnabledOnWiFiAndCellular();
  SimulateWiFiConnection();

  const GURL url("https://www.google.com/");
  const auto transition = ui::PageTransition::PAGE_TRANSITION_TYPED;

  StartPrerender(url, transition);
  EXPECT_FALSE(agent()->ValidatePrerender(url, transition));
}
