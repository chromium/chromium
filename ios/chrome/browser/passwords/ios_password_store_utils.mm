// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_password_store_utils.h"

#include "base/bind.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/store_metrics_reporter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_reuse_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&StoreMetricReporterHelper::StartMetricsReporting,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }
  ~StoreMetricReporterHelper() override = default;

 private:
  void StartMetricsReporting() {
    password_manager::PasswordStoreInterface* profile_store =
        IOSChromePasswordStoreFactory::GetForBrowserState(
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
        profile_store, /*account_store=*/nullptr, sync_service,
        identity_manager, pref_service, password_reuse_manager,
        /*is_under_advanced_protection=*/false,
        base::BindOnce(
            &StoreMetricReporterHelper::RemoveInstanceFromBrowserStateUserData,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void RemoveInstanceFromBrowserStateUserData() {
    browser_state_->RemoveUserData(kPasswordStoreMetricsReporterKey);
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
