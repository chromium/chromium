// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"

namespace {

// Creates a new instance of BrowsingDataCounter that is counting the data
// for |browser_state| related to a given deletion preference |pref_name|.
std::unique_ptr<browsing_data::BrowsingDataCounter>
CreateCounterForBrowserStateAndPref(ios::ChromeBrowserState* browser_state,
                                    base::StringPiece pref_name) {
  if (!IsNewClearBrowsingDataUIEnabled())
    return nullptr;

  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    return std::make_unique<browsing_data::HistoryCounter>(
        ios::HistoryServiceFactory::GetForBrowserStateIfExists(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS),
        base::BindRepeating(&ios::WebHistoryServiceFactory::GetForBrowserState,
                            base::Unretained(browser_state)),
        ProfileSyncServiceFactory::GetForBrowserState(browser_state));
  }

  if (pref_name == browsing_data::prefs::kDeleteCache) {
    return std::make_unique<CacheCounter>(browser_state);
  }

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    return std::make_unique<browsing_data::PasswordsCounter>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS),
        ProfileSyncServiceFactory::GetForBrowserState(browser_state));
  }

  if (pref_name == browsing_data::prefs::kDeleteFormData) {
    return std::make_unique<browsing_data::AutofillCounter>(
        ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS),
        ProfileSyncServiceFactory::GetForBrowserState(browser_state));
  }

  return nullptr;
}

}  // namespace

// static
std::unique_ptr<BrowsingDataCounterWrapper>
BrowsingDataCounterWrapper::CreateCounterWrapper(
    base::StringPiece pref_name,
    ios::ChromeBrowserState* browser_state,
    PrefService* pref_service,
    UpdateUICallback update_ui_callback) {
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter =
      CreateCounterForBrowserStateAndPref(browser_state, pref_name);
  if (!counter)
    return nullptr;

  return base::WrapUnique<BrowsingDataCounterWrapper>(
      new BrowsingDataCounterWrapper(std::move(counter), pref_service,
                                     std::move(update_ui_callback)));
}

BrowsingDataCounterWrapper::~BrowsingDataCounterWrapper() = default;

void BrowsingDataCounterWrapper::RestartCounter() {
  counter_->Restart();
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

void BrowsingDataCounterWrapper::UpdateWithResult(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  DCHECK(result);
  update_ui_callback_.Run(*result.get());
}
