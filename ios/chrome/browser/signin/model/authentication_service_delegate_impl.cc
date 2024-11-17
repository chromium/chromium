// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/authentication_service_delegate_impl.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#include "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

AuthenticationServiceDelegateImpl::AuthenticationServiceDelegateImpl(
    BrowsingDataRemover* data_remover,
    PrefService* pref_service)
    : data_remover_(CHECK_DEREF(data_remover)),
      pref_service_(CHECK_DEREF(pref_service)) {}

void AuthenticationServiceDelegateImpl::ClearBrowsingData(
    base::OnceClosure closure) {
  data_remover_->Remove(browsing_data::TimePeriod::ALL_TIME,
                        BrowsingDataRemoveMask::REMOVE_ALL, std::move(closure));
}

void AuthenticationServiceDelegateImpl::ClearBrowsingDataForSignedinPeriod(
    base::OnceClosure closure) {
  BrowsingDataRemoveMask remove_mask =
      BrowsingDataRemoveMask::REMOVE_ALL_FOR_TIME_PERIOD;

  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // If fast account switching via the account particle disk on the NTP is
    // enabled, then also close any tabs that were used since the signin. This
    // requires separately querying the tab-usage timestamps first.
    remove_mask |= BrowsingDataRemoveMask::CLOSE_TABS;
  }

  BrowsingDataRemover::RemovalParams params;
  params.keep_active_tab =
      BrowsingDataRemover::KeepActiveTabPolicy::kKeepActiveTab;

  // If `kLastSigninTimestamp` has the default base::Time() value, data will be
  // cleared for all time, which is intended to happen in this case.
  const base::Time last_signin =
      pref_service_->GetTime(prefs::kLastSigninTimestamp);
  data_remover_->RemoveInRange(last_signin, base::Time::Now(), remove_mask,
                               std::move(closure), params);
}
