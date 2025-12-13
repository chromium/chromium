// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// Records the number of impressions for a card in the magic stack
// on a per-URL basis. For example, a price drop on Tab Resumption for
// a URL with a price drop should only be shown 3 times before the card
// is auto dismissed and not shown again for that URL. Impression data
// is deleted when the user deletes the URL from history.
class ImpressionLimitService : public bookmarks::BaseBookmarkModelObserver,
                               public commerce::SubscriptionsObserver,
                               public history::HistoryServiceObserver,
                               public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // When a product is no longer price tracked, the impression should be
    // removed.
    virtual void OnUntracked(const GURL& url) {}
  };

  explicit ImpressionLimitService(PrefService* pref_service,
                                  history::HistoryService* history_service,
                                  bookmarks::BookmarkModel* bookmark_model,
                                  commerce::ShoppingService* shopping_service);

  ImpressionLimitService(const ImpressionLimitService&) = delete;
  ImpressionLimitService& operator=(const ImpressionLimitService&) = delete;

  ~ImpressionLimitService() override;

  void AddObserver(ImpressionLimitService::Observer* observer);

  void RemoveObserver(ImpressionLimitService::Observer* observer);

  // Logs a magic stack impression for a card generated using the `url` in
  // the preference called `pref_name`.
  void LogImpressionForURL(const GURL& url, const std::string_view& pref_name);

  // Get Impression count for `url` stored in `pref_name`.
  std::optional<int> GetImpressionCount(const GURL& url,
                                        const std::string_view& pref_name);

  // Logs magic stack engagement for a card generated using the 'url' in the
  // preference called 'pref_name'.
  void LogCardEngagement(const GURL& url, const std::string_view& pref_name);

  // Returns whether a card generated using the 'url' in the preference called
  // 'pref_name' has been engaged with.
  bool HasBeenEngagedWith(const GURL& url, const std::string_view& pref_name);

  // Remove entries older than 30 days. This is to avoid the store growing in an
  // unbounded way.
  void RemoveEntriesOlderThan30Days(const std::string_view& pref_name);

  // KeyedService:
  void Shutdown() override;

 private:
  friend class ImpressionLimitServiceTest;
  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override;

  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;

  // commerce::SubscriptionsObserver
  void OnSubscribe(const commerce::CommerceSubscription& subscription,
                   bool succeeded) override;
  void OnUnsubscribe(const commerce::CommerceSubscription& subscription,
                     bool succeeded) override;

  void LogImpressionForURLAtTime(const GURL& url,
                                 const std::string_view& pref_name,
                                 base::Time impression_time);

  void RemoveEntriesBeforeTime(const std::string_view& pref_name,
                               base::Time before_cutoff);

  void RemoveEntriesForURls(std::set<std::string> urls_to_remove);

  // Limit entries in any one preference to 10 at a time, Remove the oldest time
  // if we exceed the maximum.
  void RemoveOldestEntryIfSizeExceedsMaximum(const std::string_view& pref_name);

  // ImpressionLimitService::Observer
  void OnUntracked(const GURL& url);

  const std::set<std::string_view> GetAllowListedPrefs();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<commerce::ShoppingService> shopping_service_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::ScopedObservation<commerce::ShoppingService, SubscriptionsObserver>
      subscriptions_observation_{this};
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};
  base::ObserverList<ImpressionLimitService::Observer> observers_;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_H_
