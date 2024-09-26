// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/preload_controller.h"

#import <memory>

#import "base/ios/device_util.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#import "ios/chrome/browser/prerender/model/preload_controller.h"
#import "ios/chrome/browser/prerender/model/prerender_pref.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {

// Override NetworkChangeNotifier to simulate connection type changes for tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        connection_type_to_return_(
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

  TestNetworkChangeNotifier(const TestNetworkChangeNotifier&) = delete;
  TestNetworkChangeNotifier& operator=(const TestNetworkChangeNotifier&) =
      delete;

  // Simulates a change of the connection type to `type`. This will notify any
  // objects that are NetworkChangeNotifiers.
  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    connection_type_to_return_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
    base::RunLoop().RunUntilIdle();
  }

 private:
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_to_return_;
  }

  // The currently simulated network connection type. If this is set to
  // CONNECTION_NONE, then NetworkChangeNotifier::IsOffline will return true.
  net::NetworkChangeNotifier::ConnectionType connection_type_to_return_;
};

class PreloadControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(test_cbs_builder).Build();
    // Set up a NetworkChangeNotifier so that the test can simulate Wi-Fi vs.
    // cellular connection.
    network_change_notifier_.reset(new TestNetworkChangeNotifier);

    controller_ = [[PreloadController alloc] initWithProfile:profile_.get()];
  }

  // Set the "Preload webpages" setting to "Always".
  void PreloadWebpagesAlways() {
    profile_->GetPrefs()->SetInteger(
        prefs::kNetworkPredictionSetting,
        static_cast<int>(prerender_prefs::NetworkPredictionSetting::
                             kEnabledWifiAndCellular));
  }

  // Set the "Preload webpages" setting to "Only on Wi-Fi".
  void PreloadWebpagesWiFiOnly() {
    profile_->GetPrefs()->SetInteger(
        prefs::kNetworkPredictionSetting,
        static_cast<int>(
            prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly));
  }

  // Set the "Preload webpages" setting to "Never".
  void PreloadWebpagesNever() {
    profile_->GetPrefs()->SetInteger(
        prefs::kNetworkPredictionSetting,
        static_cast<int>(prerender_prefs::NetworkPredictionSetting::kDisabled));
  }

  void SimulateWiFiConnection() {
    network_change_notifier_->SimulateNetworkConnectionChange(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  void SimulateOffline() {
    network_change_notifier_->SimulateNetworkConnectionChange(
        net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  void SimulateCellularConnection() {
    network_change_notifier_->SimulateNetworkConnectionChange(
        net::NetworkChangeNotifier::CONNECTION_3G);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;

  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  PreloadController* controller_;
};

// Tests that the preload controller does not try to preload non-web urls.
TEST_F(PreloadControllerTest, DontPreloadNonWebURLs) {
  const web::Referrer kReferrer;
  const ui::PageTransition kTransition = ui::PAGE_TRANSITION_LINK;

  // Attempt to prerender an empty URL and verify that no WebState was created
  // to preload.
  [controller_ prerenderURL:GURL()
                   referrer:kReferrer
                 transition:kTransition
            currentWebState:nil
                immediately:YES];
  EXPECT_FALSE([controller_ releasePrerenderContents]);

  // Attempt to prerender the NTP and verify that no WebState was created
  // to preload.
  [controller_ prerenderURL:GURL("chrome://newtab")
                   referrer:kReferrer
                 transition:kTransition
            currentWebState:nil
                immediately:YES];
  EXPECT_FALSE([controller_ releasePrerenderContents]);

  // Attempt to prerender the flags UI and verify that no WebState was created
  // to preload.
  [controller_ prerenderURL:GURL("about:flags")
                   referrer:kReferrer
                 transition:kTransition
            currentWebState:nil
                immediately:YES];
  EXPECT_FALSE([controller_ releasePrerenderContents]);
}

TEST_F(PreloadControllerTest, TestIsPrerenderingEnabled_preloadAlways) {
  // With the "Preload Webpages" setting set to "Always", prerendering is
  // enabled regardless of network type, unless offline.
  PreloadWebpagesAlways();

  SimulateWiFiConnection();
  EXPECT_TRUE(controller_.enabled || ios::device_util::IsSingleCoreDevice() ||
              !ios::device_util::RamIsAtLeast512Mb());

  SimulateOffline();
  EXPECT_FALSE(controller_.enabled);

  SimulateCellularConnection();
  EXPECT_TRUE(controller_.enabled || ios::device_util::IsSingleCoreDevice() ||
              !ios::device_util::RamIsAtLeast512Mb());
}

TEST_F(PreloadControllerTest, TestIsPrerenderingEnabled_preloadWiFiOnly) {
  // With the Chrome "Preload Webpages" setting set to "Only on Wi-Fi",
  // prerendering is enabled only on WiFi.
  PreloadWebpagesWiFiOnly();

  SimulateWiFiConnection();
  EXPECT_TRUE(controller_.enabled || ios::device_util::IsSingleCoreDevice() ||
              !ios::device_util::RamIsAtLeast512Mb());

  SimulateOffline();
  EXPECT_FALSE(controller_.enabled);

  SimulateCellularConnection();
  EXPECT_FALSE(controller_.enabled);
}

TEST_F(PreloadControllerTest, TestIsPrerenderingEnabled_preloadNever) {
  // With the Chrome "Preload Webpages" setting set to "Never", prerendering
  // is never enabled, regardless of the network type.
  PreloadWebpagesNever();

  SimulateWiFiConnection();
  EXPECT_FALSE(controller_.enabled);

  SimulateOffline();
  EXPECT_FALSE(controller_.enabled);

  SimulateCellularConnection();
  EXPECT_FALSE(controller_.enabled);
}

TEST_F(PreloadControllerTest, PrenderingDisabledForSupervisedUsers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      supervised_user::kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS);

  // Sign in supervised user.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  AccountInfo account = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@gmail.com", signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account, identity_manager,
      /*is_subject_to_parental_controls=*/true);

  // Never prerender pages for supervised users regardless of the setting for
  // "Preload Webpages".
  SimulateWiFiConnection();

  PreloadWebpagesAlways();
  EXPECT_FALSE(controller_.enabled);

  PreloadWebpagesWiFiOnly();
  EXPECT_FALSE(controller_.enabled);

  PreloadWebpagesNever();
  EXPECT_FALSE(controller_.enabled);
}

TEST_F(PreloadControllerTest, PrenderingDisabledForSupervisedUsersWithPrefs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      supervised_user::kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS);
  supervised_user::EnableParentalControls(*profile_->GetPrefs());

  SimulateWiFiConnection();

  PreloadWebpagesAlways();
  EXPECT_FALSE(controller_.enabled);

  PreloadWebpagesWiFiOnly();
  EXPECT_FALSE(controller_.enabled);

  PreloadWebpagesNever();
  EXPECT_FALSE(controller_.enabled);
}

}  // anonymous namespace
