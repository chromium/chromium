// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_password_store_utils.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "components/password_manager/core/browser/store_metrics_reporter.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_reuse_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace password_manager {
class PasswordReuseManager;
}

namespace {

// Used for attaching metrics reporter to a WebContents.
constexpr char kPasswordStoreMetricsReporterKey[] =
    "PasswordStoreMetricsReporterKey";

class StoreMetricReporterHelper : public base::SupportsUserData::Data {
 public:
  explicit StoreMetricReporterHelper(ChromeBrowserState* browser_state)
      : browser_state_(browser_state) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&StoreMetricReporterHelper::StartMetricsReporting,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }
  ~StoreMetricReporterHelper() override = default;

 private:
  void StartMetricsReporting() {
    password_manager::PasswordStoreInterface* profile_store =
        IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    password_manager::PasswordStoreInterface* account_store =
        IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForBrowserStateIfExists(browser_state_);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForBrowserState(browser_state_);
    password_manager::PasswordReuseManager* password_reuse_manager =
        IOSChromePasswordReuseManagerFactory::GetForBrowserState(
            browser_state_);
    PrefService* pref_service = browser_state_->GetPrefs();

    metrics_reporter_ = std::make_unique<
        password_manager::StoreMetricsReporter>(
        profile_store, account_store, sync_service, identity_manager,
        pref_service, password_reuse_manager,
        /*is_under_advanced_protection=*/false,
        base::BindOnce(
            &StoreMetricReporterHelper::RemoveInstanceFromBrowserStateUserData,
            weak_ptr_factory_.GetWeakPtr()));

    [ASCredentialIdentityStore.sharedStore
        getCredentialIdentityStoreStateWithCompletion:^(
            ASCredentialIdentityStoreState* state) {
          // The completion handler sent to ASCredentialIdentityStore is
          // executed on a background thread. Putting it back onto the main
          // thread to handle the logging that requires access to the Chrome
          // browser state.
          dispatch_async(dispatch_get_main_queue(), ^{
            BOOL enabled = state.isEnabled;
            LogIfCredentialProviderEnabled(pref_service, enabled);
          });
        }];
  }

  void RemoveInstanceFromBrowserStateUserData() {
    browser_state_->RemoveUserData(kPasswordStoreMetricsReporterKey);
  }

  // Logs if the user had enabled the credential provider in their iOS settings
  // at startup. Also if the value has changed since the last launch, log the
  // new value.
  void LogIfCredentialProviderEnabled(PrefService* pref_service, BOOL enabled) {
    base::UmaHistogramBoolean("IOS.CredentialExtension.IsEnabled.Startup",
                              enabled);
    if (pref_service) {
      // The value stored on the last app startup.
      bool is_credential_provider_enabled =
          password_manager_util::IsCredentialProviderEnabledOnStartup(
              pref_service);
      // If the value changed since last launch, store the new value and log
      // that the value has changed.
      if (enabled != is_credential_provider_enabled) {
        password_manager_util::SetCredentialProviderEnabledOnStartup(
            pref_service, enabled);
        base::UmaHistogramBoolean(
            "IOS.CredentialExtension.StatusDidChangeTo.Startup", enabled);
      }
    }
  }

  ChromeBrowserState* const browser_state_;
  // StoreMetricReporterHelper is owned by the profile `metrics_reporter_` life
  // time is now bound to the profile.
  std::unique_ptr<password_manager::StoreMetricsReporter> metrics_reporter_;
  base::WeakPtrFactory<StoreMetricReporterHelper> weak_ptr_factory_{this};
};

}  // namespace

void DelayReportingPasswordStoreMetrics(ChromeBrowserState* browser_state) {
  browser_state->SetUserData(
      kPasswordStoreMetricsReporterKey,
      std::make_unique<StoreMetricReporterHelper>(browser_state));
}
