// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/testing_application_context.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "base/time/default_clock.h"
#import "base/time/default_tick_clock.h"
#import "components/network_time/network_time_tracker.h"
#import "components/os_crypt/async/browser/test_utils.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/configuration_policy_handler_list_factory.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/public/provider/chrome/browser/additional_features/additional_features_api.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"
#import "net/url_request/url_request_context_getter.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "services/network/test/test_url_loader_factory.h"

TestingApplicationContext::TestingApplicationContext()
    : application_locale_("en-US"),
      application_country_("us"),
      local_state_(nullptr),
      profile_manager_(nullptr),
      was_last_shutdown_clean_(false),
      test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()),
      test_network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()),
      variations_service_(nullptr) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);
}

TestingApplicationContext::~TestingApplicationContext() {
  DCHECK_EQ(this, GetApplicationContext());
  DCHECK(!local_state_);
  SetApplicationContext(nullptr);
}

// static
TestingApplicationContext* TestingApplicationContext::GetGlobal() {
  return static_cast<TestingApplicationContext*>(GetApplicationContext());
}

void TestingApplicationContext::SetLocalState(PrefService* local_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!local_state) {
    // The local state is owned outside of TestingApplicationContext, but
    // some of the members of TestingApplicationContext hold references to it.
    // Given our test infrastructure which tears down individual tests before
    // freeing the TestingApplicationContext, there's no good way to make the
    // local state outlive these dependencies. As a workaround, whenever
    // local state is cleared (assumedly as part of exiting the test) any
    // components owned by TestingApplicationContext that depends on the local
    // state are also freed.
    network_time_tracker_.reset();
    push_notification_service_.reset();
  }
  local_state_ = local_state;
}

void TestingApplicationContext::SetLastShutdownClean(bool clean) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  was_last_shutdown_clean_ = clean;
}

void TestingApplicationContext::SetProfileManager(ProfileManagerIOS* manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  profile_manager_ = manager;
}

void TestingApplicationContext::SetVariationsService(
    variations::VariationsService* variations_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  variations_service_ = variations_service;
}

void TestingApplicationContext::SetSystemIdentityManager(
    std::unique_ptr<SystemIdentityManager> system_identity_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!system_identity_manager_);
  system_identity_manager_ = std::move(system_identity_manager);
}

void TestingApplicationContext::SetIOSChromeIOThread(
    IOSChromeIOThread* ios_chrome_io_thread) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ios_chrome_io_thread_ = ios_chrome_io_thread;
}

void TestingApplicationContext::OnAppEnterForeground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TestingApplicationContext::OnAppEnterBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TestingApplicationContext::WasLastShutdownClean() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return was_last_shutdown_clean_;
}

PrefService* TestingApplicationContext::GetLocalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_state_;
}

net::URLRequestContextGetter*
TestingApplicationContext::GetSystemURLRequestContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestingApplicationContext::GetSharedURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return test_url_loader_factory_->GetSafeWeakWrapper();
}

network::mojom::NetworkContext*
TestingApplicationContext::GetSystemNetworkContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const std::string& TestingApplicationContext::GetApplicationLocale() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

const std::string& TestingApplicationContext::GetApplicationCountry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!application_country_.empty());
  return application_country_;
}

ProfileManagerIOS* TestingApplicationContext::GetProfileManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_manager_;
}

metrics_services_manager::MetricsServicesManager*
TestingApplicationContext::GetMetricsServicesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

metrics::MetricsService* TestingApplicationContext::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

signin::ActivePrimaryAccountsMetricsRecorder*
TestingApplicationContext::GetActivePrimaryAccountsMetricsRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

ukm::UkmRecorder* TestingApplicationContext::GetUkmRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

variations::VariationsService*
TestingApplicationContext::GetVariationsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return variations_service_;
}

net::NetLog* TestingApplicationContext::GetNetLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

net_log::NetExportFileWriter*
TestingApplicationContext::GetNetExportFileWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

network_time::NetworkTimeTracker*
TestingApplicationContext::GetNetworkTimeTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_time_tracker_) {
    DCHECK(local_state_);
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        base::WrapUnique(new base::DefaultClock),
        base::WrapUnique(new base::DefaultTickClock), local_state_, nullptr,
        std::nullopt));
  }
  return network_time_tracker_.get();
}

IOSChromeIOThread* TestingApplicationContext::GetIOSChromeIOThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ios_chrome_io_thread_.get();
}

gcm::GCMDriver* TestingApplicationContext::GetGCMDriver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

component_updater::ComponentUpdateService*
TestingApplicationContext::GetComponentUpdateService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

SafeBrowsingService* TestingApplicationContext::GetSafeBrowsingService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!fake_safe_browsing_service_) {
    fake_safe_browsing_service_ =
        base::MakeRefCounted<FakeSafeBrowsingService>();
  }
  return fake_safe_browsing_service_.get();
}

network::NetworkConnectionTracker*
TestingApplicationContext::GetNetworkConnectionTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return test_network_connection_tracker_.get();
}

BrowserPolicyConnectorIOS*
TestingApplicationContext::GetBrowserPolicyConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!browser_policy_connector_.get()) {
    browser_policy_connector_ = std::make_unique<BrowserPolicyConnectorIOS>(
        base::BindRepeating(&BuildPolicyHandlerList, true));
  }

  return browser_policy_connector_.get();
}

id<SingleSignOnService> TestingApplicationContext::GetSingleSignOnService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!single_sign_on_service_) {
    single_sign_on_service_ = ios::provider::CreateSSOService();
    DCHECK(single_sign_on_service_);
  }
  return single_sign_on_service_;
}

SystemIdentityManager* TestingApplicationContext::GetSystemIdentityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!system_identity_manager_) {
    system_identity_manager_ =
        ios::provider::CreateSystemIdentityManager(GetSingleSignOnService());
  }
  return system_identity_manager_.get();
}

AccountProfileMapper* TestingApplicationContext::GetAccountProfileMapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!account_profile_mapper_) {
    account_profile_mapper_ =
        std::make_unique<AccountProfileMapper>(GetSystemIdentityManager());
  }
  return account_profile_mapper_.get();
}

IncognitoSessionTracker*
TestingApplicationContext::GetIncognitoSessionTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nullptr;
}

PushNotificationService*
TestingApplicationContext::GetPushNotificationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!push_notification_service_) {
    push_notification_service_ = ios::provider::CreatePushNotificationService();
    DCHECK(push_notification_service_);
  }

  return push_notification_service_.get();
}

os_crypt_async::OSCryptAsync* TestingApplicationContext::GetOSCryptAsync() {
  if (!os_crypt_async_) {
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
  }
  return os_crypt_async_.get();
}

AdditionalFeaturesController*
TestingApplicationContext::GetAdditionalFeaturesController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!additional_features_controller_) {
    additional_features_controller_ =
        ios::provider::CreateAdditionalFeaturesController();
  }
  return additional_features_controller_.get();
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
optimization_guide::OnDeviceModelServiceController*
TestingApplicationContext::GetOnDeviceModelServiceController(
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        on_device_component_manager) {
  return nullptr;
}
#endif  // BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE
