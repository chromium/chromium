// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service.h"

#include <set>

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_prefs.h"
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

}  // namespace

ImpressionLimitService::ImpressionLimitService(
    PrefService* pref_service,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model,
    commerce::ShoppingService* shopping_service)
    : pref_service_(pref_service),
      history_service_(history_service),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service) {
  DCHECK(history_service_);
  if (base::FeatureList::IsEnabled(commerce::kShopCardImpressionLimits)) {
    history_service_observation_.Observe(history_service_.get());
    subscriptions_observation_.Observe(shopping_service_);
    bookmark_model_observation_.Observe(bookmark_model);
    for (const auto& pref_name : GetAllowListedPrefs()) {
      RemoveEntriesOlderThan30Days(pref_name);
    }
  } else {
    // ShopCard feature is experimental. Don't keep impression
    // counts around when flag is turned off.
    for (const auto& pref_name : GetAllowListedPrefs()) {
      pref_service_->ClearPref(pref_name);
    }
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
  if (deletion_info.IsAllHistory()) {
    for (const auto& pref_name : GetAllowListedPrefs()) {
      pref_service_->ClearPref(pref_name);
    }
  } else {
    std::set<std::string> urls_to_remove;
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      urls_to_remove.insert(GetUrlKey(row.url()).spec());
    }
    RemoveEntriesForURls(urls_to_remove);
  }
}

void ImpressionLimitService::BookmarkModelChanged() {}

void ImpressionLimitService::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  std::set<std::string> urls_to_remove;
  for (const GURL& url : no_longer_bookmarked) {
    urls_to_remove.insert(GetUrlKey(url).spec());
  }
  RemoveEntriesForURls(urls_to_remove);
}

void ImpressionLimitService::OnSubscribe(
    const commerce::CommerceSubscription& subscription,
    bool succeeded) {}

void ImpressionLimitService::OnUnsubscribe(
    const commerce::CommerceSubscription& subscription,
    bool succeeded) {
  if (!succeeded) {
    return;
  }
  if (subscription.id_type != commerce::IdentifierType::kProductClusterId) {
    return;
  }

  uint64_t cluster_id;
  if (!base::StringToUint64(subscription.id, &cluster_id)) {
    return;
  }
  std::set<std::string> urls_to_remove;

  // Note not associating bookmarks w/ cluster id.
  for (auto* node :
       commerce::GetBookmarksWithClusterId(bookmark_model_, cluster_id)) {
    urls_to_remove.insert(GetUrlKey(node->url()).spec());
  }
  RemoveEntriesForURls(urls_to_remove);
}

void ImpressionLimitService::LogImpressionForURL(
    const GURL& url,
    const std::string_view& pref_name) {
  if (!GetAllowListedPrefs().contains(pref_name)) {
    NOTREACHED() << pref_name
                 << " must be registered with ImpressionLimitService";
  }
  LogImpressionForURLAtTime(url, pref_name, base::Time::Now());
}

std::optional<int> ImpressionLimitService::GetImpressionCount(
    const GURL& url,
    const std::string_view& pref_name) {
  if (!GetAllowListedPrefs().contains(pref_name)) {
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

void ImpressionLimitService::RemoveEntriesForURls(
    std::set<std::string> urls_to_remove) {
  for (const auto& pref_name : GetAllowListedPrefs()) {
    base::Value::Dict impressions = pref_service_->GetDict(pref_name).Clone();

    for (const auto& url : urls_to_remove) {
      impressions.Remove(url);
    }
    pref_service_->SetDict(pref_name, std::move(impressions));
  }
}

const std::set<std::string_view> ImpressionLimitService::GetAllowListedPrefs() {
  static std::set<std::string_view> prefs = {
      tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions,
      shop_card_prefs::kShopCardPriceDropUrlImpressions};
  return prefs;
}
