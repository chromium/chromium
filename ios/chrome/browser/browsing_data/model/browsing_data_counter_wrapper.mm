// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_counter_wrapper.h"

#import <string_view>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "components/browsing_data/core/counters/autofill_counter.h"
#import "components/browsing_data/core/counters/browsing_data_counter.h"
#import "components/browsing_data/core/counters/history_counter.h"
#import "components/browsing_data/core/counters/passwords_counter.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/browsing_data/model/cache_counter.h"
#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/web_history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

namespace {

// Creates a new instance of BrowsingDataCounter that is counting the data
// for `profile` related to a given deletion preference `pref_name`.
std::unique_ptr<browsing_data::BrowsingDataCounter>
CreateCounterForProfileAndPref(ProfileIOS* profile,
                               std::string_view pref_name) {
  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    return std::make_unique<browsing_data::HistoryCounter>(
        ios::HistoryServiceFactory::GetForProfileIfExists(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        base::BindRepeating(&ios::WebHistoryServiceFactory::GetForProfile,
                            base::Unretained(profile)),
        SyncServiceFactory::GetForProfile(profile));
  }

  if (pref_name == browsing_data::prefs::kDeleteCache) {
    return std::make_unique<CacheCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    return std::make_unique<browsing_data::PasswordsCounter>(
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        IOSChromeAccountPasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  }

  if (pref_name == browsing_data::prefs::kDeleteFormData) {
    return std::make_unique<browsing_data::AutofillCounter>(
        autofill::PersonalDataManagerFactory::GetForProfile(profile),
        ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        /*user_annotations_service=*/nullptr,
        SyncServiceFactory::GetForProfile(profile));
  }

  if (pref_name == browsing_data::prefs::kCloseTabs) {
    return std::make_unique<TabsCounter>(
        BrowserListFactory::GetForProfile(profile),
        SessionRestorationServiceFactory::GetForProfile(profile));
  }

  return nullptr;
}

}  // namespace

// static
std::unique_ptr<BrowsingDataCounterWrapper>
BrowsingDataCounterWrapper::CreateCounterWrapper(
    std::string_view pref_name,
    ProfileIOS* profile,
    PrefService* pref_service,
    UpdateUICallback update_ui_callback) {
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter =
      CreateCounterForProfileAndPref(profile, pref_name);
  if (!counter) {
    return nullptr;
  }

  return base::WrapUnique<BrowsingDataCounterWrapper>(
      new BrowsingDataCounterWrapper(std::move(counter), pref_service,
                                     std::move(update_ui_callback)));
}

// static
std::unique_ptr<BrowsingDataCounterWrapper>
BrowsingDataCounterWrapper::CreateCounterWrapper(
    std::string_view pref_name,
    ProfileIOS* profile,
    PrefService* pref_service,
    base::Time begin_time,
    UpdateUICallback update_ui_callback) {
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter =
      CreateCounterForProfileAndPref(profile, pref_name);
  if (!counter) {
    return nullptr;
  }

  return base::WrapUnique<BrowsingDataCounterWrapper>(
      new BrowsingDataCounterWrapper(std::move(counter), pref_service,
                                     begin_time,
                                     std::move(update_ui_callback)));
}

BrowsingDataCounterWrapper::~BrowsingDataCounterWrapper() = default;

void BrowsingDataCounterWrapper::RestartCounter() {
  counter_->Restart();
}

void BrowsingDataCounterWrapper::SetBeginTime(base::Time beginTime) {
  counter_->SetBeginTime(beginTime);
}

BrowsingDataCounterWrapper::BrowsingDataCounterWrapper(
    std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
    PrefService* pref_service,
    UpdateUICallback update_ui_callback)
    : counter_(std::move(counter)),
      update_ui_callback_(std::move(update_ui_callback)) {
  DCHECK(counter_);
  DCHECK(update_ui_callback_);
  counter_->Init(
      pref_service, browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::BindRepeating(&BrowsingDataCounterWrapper::UpdateWithResult,
                          base::Unretained(this)));
}

BrowsingDataCounterWrapper::BrowsingDataCounterWrapper(
    std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
    PrefService* pref_service,
    base::Time begin_time,
    UpdateUICallback update_ui_callback)
    : counter_(std::move(counter)),
      update_ui_callback_(std::move(update_ui_callback)) {
  DCHECK(counter_);
  DCHECK(update_ui_callback_);
  counter_->InitWithoutPeriodPref(
      pref_service, browsing_data::ClearBrowsingDataTab::ADVANCED, begin_time,
      base::BindRepeating(&BrowsingDataCounterWrapper::UpdateWithResult,
                          base::Unretained(this)));
}

void BrowsingDataCounterWrapper::UpdateWithResult(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  DCHECK(result);
  update_ui_callback_.Run(*result.get());
}
