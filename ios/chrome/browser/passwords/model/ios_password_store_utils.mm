// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_password_store_utils.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/store_metrics_reporter.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_reuse_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_password_manager_settings_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
  explicit StoreMetricReporterHelper(ProfileIOS* profile) : profile_(profile) {
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
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    password_manager::PasswordStoreInterface* account_store =
        IOSChromeAccountPasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfileIfExists(profile_);
    password_manager::PasswordReuseManager* password_reuse_manager =
        IOSChromePasswordReuseManagerFactory::GetForProfile(profile_);
    password_manager::PasswordManagerSettingsService* settings =
        IOSPasswordManagerSettingsServiceFactory::GetForProfile(profile_);

    PrefService* pref_service = profile_->GetPrefs();

    metrics_reporter_ =
        std::make_unique<password_manager::StoreMetricsReporter>(
            profile_store, account_store, sync_service, pref_service,
            password_reuse_manager, settings,
            base::BindOnce(
                &StoreMetricReporterHelper::RemoveInstanceFromProfileUserData,
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

  void RemoveInstanceFromProfileUserData() {
    profile_->RemoveUserData(kPasswordStoreMetricsReporterKey);
  }

  // Logs if the user had enabled the credential provider in their iOS settings
  // at startup. Also if the value has changed since the last launch, log the
  // new value.
  void LogIfCredentialProviderEnabled(PrefService* pref_service, BOOL enabled) {
    base::UmaHistogramBoolean("IOS.CredentialExtension.IsEnabled.Startup",
                              enabled);
    PrefService* local_state = GetApplicationContext()->GetLocalState();
    // The value stored on the last app startup.
    bool is_credential_provider_enabled =
        password_manager_util::IsCredentialProviderEnabledOnStartup(
            local_state);
    // If the value changed since last launch, store the new value and log
    // that the value has changed.
    if (enabled != is_credential_provider_enabled) {
      password_manager_util::SetCredentialProviderEnabledOnStartup(local_state,
                                                                   enabled);
      base::UmaHistogramBoolean(
          "IOS.CredentialExtension.StatusDidChangeTo.Startup", enabled);
    }
  }

  const raw_ptr<ProfileIOS> profile_;
  // StoreMetricReporterHelper is owned by the profile `metrics_reporter_` life
  // time is now bound to the profile.
  std::unique_ptr<password_manager::StoreMetricsReporter> metrics_reporter_;
  base::WeakPtrFactory<StoreMetricReporterHelper> weak_ptr_factory_{this};
};

}  // namespace

void DelayReportingPasswordStoreMetrics(ProfileIOS* profile) {
  profile->SetUserData(kPasswordStoreMetricsReporterKey,
                       std::make_unique<StoreMetricReporterHelper>(profile));
}
