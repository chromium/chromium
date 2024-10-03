// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPLICATION_CONTEXT_MODEL_APPLICATION_CONTEXT_IMPL_H_
#define IOS_CHROME_BROWSER_APPLICATION_CONTEXT_MODEL_APPLICATION_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace base {
class CommandLine;
class SequencedTaskRunner;
}  // namespace base

class ApplicationBreadcrumbsLogger;

namespace network {
class NetworkChangeManager;
}

class ApplicationContextImpl : public ApplicationContext {
 public:
  ApplicationContextImpl(base::SequencedTaskRunner* local_state_task_runner,
                         const base::CommandLine& command_line,
                         const std::string& locale,
                         const std::string& country);

  ApplicationContextImpl(const ApplicationContextImpl&) = delete;
  ApplicationContextImpl& operator=(const ApplicationContextImpl&) = delete;

  ~ApplicationContextImpl() override;

  // Called before the browser threads are created.
  void PreCreateThreads();

  // Called after the browser threads are created.
  void PostCreateThreads();

  // Called after the threads have been created but before the message loops
  // starts running. Allows the ApplicationContext to do any initialization
  // that requres all threads running.
  void PreMainMessageLoopRun();

  // Most cleanup is done by these functions, driven from IOSChromeMainParts
  // rather than in the destructor, so that we can interleave cleanup with
  // threads being stopped.
  void StartTearDown();
  void PostDestroyThreads();

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
  // Represents the possible application states the app can be in.
  enum class AppState {
    kForeground,
    kBackground,
  };

  // Helper method to implement the work required when transitioning between
  // application states.
  void OnAppEnterState(AppState app_state);

  // Sets the locale used by the application.
  void SetApplicationLocale(const std::string& locale);

  // Create the local state.
  void CreateLocalState();

  // Create the gcm driver.
  void CreateGCMDriver();

  SEQUENCE_CHECKER(sequence_checker_);

  // Used internally for tracking whether the call to StartTearDown() has
  // happened already, to avoid recreating lazily-constructed objects after they
  // have already been destroyed.
  bool tearing_down_ = false;

  // Logger which observers and logs application wide events to breadcrumbs.
  // Will be null if breadcrumbs feature is not enabled.
  std::unique_ptr<ApplicationBreadcrumbsLogger> application_breadcrumbs_logger_;

  // Must be destroyed after `local_state_`. BrowserStatePolicyConnector isn't a
  // keyed service because the pref service, which isn't a keyed service, has a
  // hard dependency on the policy infrastructure. In order to outlive the pref
  // service, the policy connector must live outside the keyed services.
  std::unique_ptr<BrowserPolicyConnectorIOS> browser_policy_connector_;

  // Must be destroyed after `profile_manager_` as some of the KeyedService
  // register themselves as NetworkConnectionObserver and need to unregister
  // themselves before NetworkConnectionTracker destruction. Must also be
  // destroyed after `gcm_driver_` and `metrics_services_manager_` since these
  // own objects that register themselves as NetworkConnectionObservers.
  std::unique_ptr<network::NetworkChangeManager> network_change_manager_;
  std::unique_ptr<network::NetworkConnectionTracker>
      network_connection_tracker_;

  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<net_log::NetExportFileWriter> net_export_file_writer_;
  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  std::unique_ptr<IOSChromeIOThread> ios_chrome_io_thread_;
  std::unique_ptr<signin::ActivePrimaryAccountsMetricsRecorder>
      active_primary_accounts_metrics_recorder_;
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;
  std::unique_ptr<component_updater::ComponentUpdateService> component_updater_;

  std::unique_ptr<ProfileManagerIOS> profile_manager_;
  std::string application_locale_;
  std::string application_country_;

  // Sequenced task runner for local state related I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> local_state_task_runner_;

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

  __strong id<SingleSignOnService> single_sign_on_service_ = nil;
  std::unique_ptr<SystemIdentityManager> system_identity_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;

  std::unique_ptr<IncognitoSessionTracker> incognito_session_tracker_;
  std::unique_ptr<PushNotificationService> push_notification_service_;

  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;

  std::unique_ptr<AdditionalFeaturesController> additional_features_controller_;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  scoped_refptr<optimization_guide::OnDeviceModelServiceController>
      on_device_model_service_controller_;
#endif  // BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_MODEL_APPLICATION_CONTEXT_IMPL_H_
