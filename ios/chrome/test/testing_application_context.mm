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
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"
#import "net/url_request/url_request_context_getter.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "services/network/test/test_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestingApplicationContext::TestingApplicationContext()
    : application_locale_("en"),
      application_country_("us"),
      local_state_(nullptr),
      chrome_browser_state_manager_(nullptr),
      was_last_shutdown_clean_(false),
      test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()),
      test_network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()) {
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
  DCHECK(thread_checker_.CalledOnValidThread());
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
  }
  local_state_ = local_state;
}

void TestingApplicationContext::SetLastShutdownClean(bool clean) {
  DCHECK(thread_checker_.CalledOnValidThread());
  was_last_shutdown_clean_ = clean;
}

void TestingApplicationContext::SetChromeBrowserStateManager(
    ios::ChromeBrowserStateManager* manager) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chrome_browser_state_manager_ = manager;
}

void TestingApplicationContext::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestingApplicationContext::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool TestingApplicationContext::WasLastShutdownClean() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return was_last_shutdown_clean_;
}

PrefService* TestingApplicationContext::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_state_;
}

net::URLRequestContextGetter*
TestingApplicationContext::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestingApplicationContext::GetSharedURLLoaderFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return test_url_loader_factory_->GetSafeWeakWrapper();
}

network::mojom::NetworkContext*
TestingApplicationContext::GetSystemNetworkContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTREACHED();
  return nullptr;
}

const std::string& TestingApplicationContext::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

const std::string& TestingApplicationContext::GetApplicationCountry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_country_.empty());
  return application_country_;
}

ios::ChromeBrowserStateManager*
TestingApplicationContext::GetChromeBrowserStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return chrome_browser_state_manager_;
}

metrics_services_manager::MetricsServicesManager*
TestingApplicationContext::GetMetricsServicesManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

metrics::MetricsService* TestingApplicationContext::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

ukm::UkmRecorder* TestingApplicationContext::GetUkmRecorder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

variations::VariationsService*
TestingApplicationContext::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

net::NetLog* TestingApplicationContext::GetNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

net_log::NetExportFileWriter*
TestingApplicationContext::GetNetExportFileWriter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

network_time::NetworkTimeTracker*
TestingApplicationContext::GetNetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_time_tracker_) {
    DCHECK(local_state_);
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        base::WrapUnique(new base::DefaultClock),
        base::WrapUnique(new base::DefaultTickClock), local_state_, nullptr));
  }
  return network_time_tracker_.get();
}

IOSChromeIOThread* TestingApplicationContext::GetIOSChromeIOThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

gcm::GCMDriver* TestingApplicationContext::GetGCMDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

component_updater::ComponentUpdateService*
TestingApplicationContext::GetComponentUpdateService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

SafeBrowsingService* TestingApplicationContext::GetSafeBrowsingService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!fake_safe_browsing_service_) {
    fake_safe_browsing_service_ =
        base::MakeRefCounted<FakeSafeBrowsingService>();
  }
  return fake_safe_browsing_service_.get();
}

network::NetworkConnectionTracker*
TestingApplicationContext::GetNetworkConnectionTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return test_network_connection_tracker_.get();
}

BrowserPolicyConnectorIOS*
TestingApplicationContext::GetBrowserPolicyConnector() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!browser_policy_connector_.get()) {
    browser_policy_connector_ = std::make_unique<BrowserPolicyConnectorIOS>(
        base::BindRepeating(&BuildPolicyHandlerList, true));
  }

  return browser_policy_connector_.get();
}

id<SingleSignOnService> TestingApplicationContext::GetSSOService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!single_sign_on_service_) {
    single_sign_on_service_ = ios::provider::CreateSSOService();
    DCHECK(single_sign_on_service_);
  }
  return single_sign_on_service_;
}

SystemIdentityManager* TestingApplicationContext::GetSystemIdentityManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!system_identity_manager_) {
    system_identity_manager_ =
        ios::provider::CreateSystemIdentityManager(GetSSOService());
  }
  return system_identity_manager_.get();
}

segmentation_platform::OTRWebStateObserver*
TestingApplicationContext::GetSegmentationOTRWebStateObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

PushNotificationService*
TestingApplicationContext::GetPushNotificationService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!push_notification_service_) {
    push_notification_service_ = ios::provider::CreatePushNotificationService();
    DCHECK(push_notification_service_);
  }

  return push_notification_service_.get();
}
