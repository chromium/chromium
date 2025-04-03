// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service.h"

#include <set>

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/history_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "url/gurl.h"

namespace {

GURL GetUrlKey(GURL url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

const std::set<std::string_view> kWhitelistedPrefs = {
    tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions};

}  // namespace

ImpressionLimitService::ImpressionLimitService(
    PrefService* pref_service,
    history::HistoryService* history_service)
    : history_service_(history_service), pref_service_(pref_service) {
  DCHECK(history_service_);
  history_service_observation_.Observe(history_service_.get());
  for (const auto pref_name : kWhitelistedPrefs) {
    RemoveEntriesOlderThan30Days(pref_name);
  }
}

ImpressionLimitService::~ImpressionLimitService() = default;

void ImpressionLimitService::Shutdown() {
  if (history_service_) {
    history_service_observation_.Reset();
  }
  history_service_ = nullptr;
}

void ImpressionLimitService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/407527797) Remove impressions for a URL
  // when the URL is deleted from history.
}

void ImpressionLimitService::LogImpressionForURL(
    const GURL& url,
    const std::string_view& pref_name) {
  if (!kWhitelistedPrefs.contains(pref_name)) {
    NOTREACHED() << pref_name
                 << " must be registered with ImpressionLimitService";
  }
  LogImpressionForURLAtTime(url, pref_name, base::Time::Now());
}

std::optional<int> ImpressionLimitService::GetImpressionCount(
    const GURL& url,
    const std::string_view& pref_name) {
  if (!kWhitelistedPrefs.contains(pref_name)) {
    NOTREACHED() << pref_name
                 << " must be registered with ImpressionLimitService";
  }
  const base::Value::Dict& impressions = pref_service_->GetDict(pref_name);

  const base::Value::List* impressions_data =
      impressions.FindList(GetUrlKey(url).spec());

  base::Value::List impressions_data_update;
  if (impressions_data) {
    return (*impressions_data)[0].GetInt();
  }
  return std::nullopt;
}

void ImpressionLimitService::RemoveEntriesOlderThan30Days(
    const std::string_view& pref_name) {
  RemoveEntriesBeforeTime(pref_name, base::Time::Now() - base::Days(30));
}

void ImpressionLimitService::LogImpressionForURLAtTime(
    const GURL& url,
    const std::string_view& pref_name,
    base::Time impression_time) {
  std::string url_key = GetUrlKey(url).spec();
  base::Value::Dict impressions = pref_service_->GetDict(pref_name).Clone();

  const base::Value::List* impressions_data = impressions.FindList(url_key);

  base::Value::List impressions_data_update;
  if (impressions_data) {
    impressions_data_update.Append((*impressions_data)[0].GetInt() + 1);
    impressions_data_update.Append((*impressions_data)[1].Clone());
  } else {
    impressions_data_update.Append(1);
    impressions_data_update.Append(base::TimeToValue(impression_time));
  }
  impressions.Set(url_key, std::move(impressions_data_update));
  pref_service_->SetDict(pref_name, std::move(impressions));
}

void ImpressionLimitService::RemoveEntriesBeforeTime(
    const std::string_view& pref_name,
    base::Time before_cutoff) {
  base::Value::Dict impressions = pref_service_->GetDict(pref_name).Clone();

  std::set<std::string> urls_to_remove;
  for (const auto impression : impressions) {
    const std::string& url = impression.first;
    const base::Value::List& impressions_data = impression.second.GetList();
    base::Time impression_time = base::ValueToTime(impressions_data[1]).value();
    if (impression_time <= before_cutoff) {
      urls_to_remove.insert(url);
    }
  }
  for (const auto& url : urls_to_remove) {
    impressions.Remove(url);
  }
  pref_service_->SetDict(pref_name, std::move(impressions));
}
