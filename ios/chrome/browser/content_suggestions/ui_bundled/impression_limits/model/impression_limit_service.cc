// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/model/impression_limit_service.h"

#include <set>

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/model/shop_card_prefs.h"
#include "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "url/gurl.h"

namespace {

const size_t kMaxEntriesPerPreference = 10;
const size_t kMaxUrlLength = 1024;

std::string GetUrlKey(GURL url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  std::string shortened = url.ReplaceComponents(replacements).spec();
  if (shortened.length() > kMaxUrlLength) {
    shortened = shortened.substr(0, kMaxUrlLength);
  }
  return shortened;
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
  history_service_observation_.Observe(history_service_.get());
  subscriptions_observation_.Observe(shopping_service_);
  bookmark_model_observation_.Observe(bookmark_model);
  for (const auto& pref_name : GetAllowListedPrefs()) {
    RemoveEntriesOlderThan30Days(pref_name);
  }
  // ShopCard arm 3, 4 and 5 are still experimental (only arm 1 has launched).
  // So delete any preferences stored for those arms, unless the arm is turned
  // on.
  if (commerce::kShopCardVariation.Get() != commerce::kShopCardArm3) {
    pref_service_->ClearPref(
        tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions);
  }
  if (commerce::kShopCardVariation.Get() != commerce::kShopCardArm4) {
    pref_service_->ClearPref(
        tab_resumption_prefs::kTabResumptionWithPriceTrackableUrlImpressions);
  }
  if (commerce::kShopCardVariation.Get() != commerce::kShopCardArm5) {
    pref_service_->ClearPref(
        tab_resumption_prefs::kTabResumptionRegularUrlImpressions);
  }
}

ImpressionLimitService::~ImpressionLimitService() = default;

void ImpressionLimitService::AddObserver(
    ImpressionLimitService::Observer* observer) {
  observers_.AddObserver(observer);
}

void ImpressionLimitService::RemoveObserver(
    ImpressionLimitService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

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
      urls_to_remove.insert(GetUrlKey(row.url()));
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
    urls_to_remove.insert(GetUrlKey(url));
    OnUntracked(url);
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
    urls_to_remove.insert(GetUrlKey(node->url()));
    OnUntracked(node->url());
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
  size_t storage_size_kb =
      pref_service_->GetDict(pref_name).EstimateMemoryUsage() / 1024;
  if (pref_name == shop_card_prefs::kShopCardPriceDropUrlImpressions) {
    base::UmaHistogramMemoryKB(
        "IOS.MagicStack.ShopCard.PriceDropOnTrackedItem."
        "ImpressionLimitStorageSize",
        storage_size_kb);
  } else if (pref_name ==
             tab_resumption_prefs::kTabResumptionRegularUrlImpressions) {
    base::UmaHistogramMemoryKB(
        "IOS.MagicStack.ShopCard.TabResumptionRegular."
        "ImpressionLimitStorageSize",
        storage_size_kb);
  } else if (pref_name ==
             tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions) {
    base::UmaHistogramMemoryKB(
        "IOS.MagicStack.ShopCard.TabResumptionWithPriceDrop."
        "ImpressionLimitStorageSize",
        storage_size_kb);
  } else if (pref_name == tab_resumption_prefs::
                              kTabResumptionWithPriceTrackableUrlImpressions) {
    base::UmaHistogramMemoryKB(
        "IOS.MagicStack.ShopCard.TabResumptionWithPriceTracking."
        "ImpressionLimitStorageSize",
        storage_size_kb);
  }
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
      impressions.FindList(GetUrlKey(url));

  base::Value::List impressions_data_update;
  if (impressions_data) {
    return (*impressions_data)[0].GetInt();
  }
  return std::nullopt;
}

void ImpressionLimitService::LogCardEngagement(
    const GURL& url,
    const std::string_view& pref_name) {
  if (!GetAllowListedPrefs().contains(pref_name)) {
    NOTREACHED() << pref_name
                 << " must be registered with ImpressionLimitService";
  }
  std::string url_key = GetUrlKey(url);
  base::Value::Dict impressions = pref_service_->GetDict(pref_name).Clone();

  const base::Value::List* impressions_data = impressions.FindList(url_key);
  base::Value::List impressions_data_update;
  if (impressions_data) {
    // Impression count for card.
    impressions_data_update.Append((*impressions_data)[0].Clone());
    // Time of first impression.
    impressions_data_update.Append((*impressions_data)[1].Clone());

  } else {
    // Impression count for card.
    impressions_data_update.Append(1);
    // Time of first impression.
    impressions_data_update.Append(base::TimeToValue(base::Time::Now()));
  }
  // Whether card has been tapped.
  impressions_data_update.Append(true);

  impressions.Set(url_key, std::move(impressions_data_update));
  pref_service_->SetDict(pref_name, std::move(impressions));
  RemoveOldestEntryIfSizeExceedsMaximum(pref_name);
}

bool ImpressionLimitService::HasBeenEngagedWith(
    const GURL& url,
    const std::string_view& pref_name) {
  if (!GetAllowListedPrefs().contains(pref_name)) {
    NOTREACHED() << pref_name
                 << " must be registered with ImpressionLimitService";
  }
  std::string url_key = GetUrlKey(url);
  const base::Value::Dict& impressions = pref_service_->GetDict(pref_name);

  const base::Value::List* impressions_data = impressions.FindList(url_key);
  if (impressions_data) {
    return (*impressions_data)[2].GetBool();
  }
  return false;
}

void ImpressionLimitService::RemoveEntriesOlderThan30Days(
    const std::string_view& pref_name) {
  RemoveEntriesBeforeTime(pref_name, base::Time::Now() - base::Days(30));
}

void ImpressionLimitService::LogImpressionForURLAtTime(
    const GURL& url,
    const std::string_view& pref_name,
    base::Time impression_time) {
  std::string url_key = GetUrlKey(url);
  base::Value::Dict impressions = pref_service_->GetDict(pref_name).Clone();

  const base::Value::List* impressions_data = impressions.FindList(url_key);

  base::Value::List impressions_data_update;
  if (impressions_data) {
    // Impression count for card.
    impressions_data_update.Append((*impressions_data)[0].GetInt() + 1);
    // Time of first impression.
    impressions_data_update.Append((*impressions_data)[1].Clone());
    // Whether card has been tapped.
    impressions_data_update.Append((*impressions_data)[2].Clone());
  } else {
    // Impression count for card.
    impressions_data_update.Append(1);
    // Time of first impression.
    impressions_data_update.Append(base::TimeToValue(impression_time));
    // Whether card has been tapped.
    impressions_data_update.Append(false);
  }
  impressions.Set(url_key, std::move(impressions_data_update));
  pref_service_->SetDict(pref_name, std::move(impressions));
  RemoveOldestEntryIfSizeExceedsMaximum(pref_name);
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

void ImpressionLimitService::RemoveOldestEntryIfSizeExceedsMaximum(
    const std::string_view& pref_name) {
  if (pref_service_->GetDict(pref_name).size() <= kMaxEntriesPerPreference) {
    return;
  }
  base::Value::Dict impressions = pref_service_->GetDict(pref_name).Clone();
  // Find URL for earliest entry
  std::optional<std::pair<std::string, base::Time>> smallest = std::nullopt;
  for (const auto impression : impressions) {
    const base::Value::List& impressions_data = impression.second.GetList();
    base::Time impression_time = base::ValueToTime(impressions_data[1]).value();

    if (!smallest.has_value() || impression_time < smallest.value().second) {
      smallest = {impression.first, impression_time};
    }
  }
  // Remove earliest entry.
  if (smallest.has_value()) {
    impressions.Remove(smallest->first);
    pref_service_->SetDict(pref_name, std::move(impressions));
  }
}

void ImpressionLimitService::OnUntracked(const GURL& url) {
  for (auto& observer : observers_) {
    observer.OnUntracked(url);
  }
}

const std::set<std::string_view> ImpressionLimitService::GetAllowListedPrefs() {
  static base::NoDestructor<std::set<std::string_view>> prefs(
      {tab_resumption_prefs::kTabResumptionRegularUrlImpressions,
       tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions,
       tab_resumption_prefs::kTabResumptionWithPriceTrackableUrlImpressions,
       shop_card_prefs::kShopCardPriceDropUrlImpressions});
  return *prefs;
}
