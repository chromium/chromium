// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
#define IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace network {
class TestNetworkConnectionTracker;
class TestURLLoaderFactory;
}  // namespace network

class MockPromosManager;

class TestingApplicationContext : public ApplicationContext {
 public:
  TestingApplicationContext();

  TestingApplicationContext(const TestingApplicationContext&) = delete;
  TestingApplicationContext& operator=(const TestingApplicationContext&) =
      delete;

  ~TestingApplicationContext() override;

  // Convenience method to get the current application context as a
  // TestingApplicationContext.
  static TestingApplicationContext* GetGlobal();

  // Sets the local state.
  void SetLocalState(PrefService* local_state);

  // Sets the last shutdown "clean" state.
  void SetLastShutdownClean(bool clean);

  // Sets the ProfileManager.
  void SetProfileManager(ProfileManagerIOS* manager);

  // Sets the VariationsService.
  void SetVariationsService(variations::VariationsService* variations_service);

  // Sets the SystemIdentityManager.
  // Must be set before `GetSystemIdentityManager` is called (i.e. before
  // creating a TestProfileIOS).
  void SetSystemIdentityManager(
      std::unique_ptr<SystemIdentityManager> system_identity_manager);

  // Sets the IOSChromeIOThread.
  void SetIOSChromeIOThread(IOSChromeIOThread* ios_chrome_io_thread);

  // ApplicationContext implementation.
  void OnAppEnterForeground() override;
  void OnAppEnterBackground() override;
  bool WasLastShutdownClean() override;

  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  const std::string& GetApplicationLocale() override;
  const std::string& GetApplicationCountry() override;
  ProfileManagerIOS* GetProfileManager() override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* GetMetricsService() override;
  signin::ActivePrimaryAccountsMetricsRecorder*
  GetActivePrimaryAccountsMetricsRecorder() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  variations::VariationsService* GetVariationsService() override;
  net::NetLog* GetNetLog() override;
  net_log::NetExportFileWriter* GetNetExportFileWriter() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  IOSChromeIOThread* GetIOSChromeIOThread() override;
  gcm::GCMDriver* GetGCMDriver() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  SafeBrowsingService* GetSafeBrowsingService() override;
  network::NetworkConnectionTracker* GetNetworkConnectionTracker() override;
  BrowserPolicyConnectorIOS* GetBrowserPolicyConnector() override;
  id<SingleSignOnService> GetSingleSignOnService() override;
  SystemIdentityManager* GetSystemIdentityManager() override;
  AccountProfileMapper* GetAccountProfileMapper() override;
  IncognitoSessionTracker* GetIncognitoSessionTracker() override;
  PushNotificationService* GetPushNotificationService() override;
  os_crypt_async::OSCryptAsync* GetOSCryptAsync() override;
  AdditionalFeaturesController* GetAdditionalFeaturesController() override;
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::OnDeviceModelServiceController*
  GetOnDeviceModelServiceController(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          on_device_component_manager) override;
#endif  // BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::string application_locale_;
  std::string application_country_;
  raw_ptr<PrefService> local_state_;

  // Must be destroyed after `local_state_`. profilePolicyConnector isn't a
  // keyed service because the pref service, which isn't a keyed service, has a
  // hard dependency on the policy infrastructure. In order to outlive the pref
  // service, the policy connector must live outside the keyed services.
  std::unique_ptr<BrowserPolicyConnectorIOS> browser_policy_connector_;
  std::unique_ptr<MockPromosManager> promos_manager_;

  raw_ptr<ProfileManagerIOS> profile_manager_;
  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  bool was_last_shutdown_clean_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<SafeBrowsingService> fake_safe_browsing_service_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      test_network_connection_tracker_;
  __strong id<SingleSignOnService> single_sign_on_service_ = nil;
  std::unique_ptr<SystemIdentityManager> system_identity_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;
  std::unique_ptr<PushNotificationService> push_notification_service_;
  raw_ptr<variations::VariationsService> variations_service_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<AdditionalFeaturesController> additional_features_controller_;
  raw_ptr<IOSChromeIOThread> ios_chrome_io_thread_;
};

#endif  // IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
