// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_IMPRESSION_LIMIT_SERVICE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_IMPRESSION_LIMIT_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class PrefService;

// Records the number of impressions for a card in the magic stack
// on a per-URL basis. For example, a price drop on Tab Resumption for
// a URL with a price drop should only be shown 3 times before the card
// is auto dismissed and not shown again for that URL. Impression data
// is deleted when the user deletes the URL from history.
class ImpressionLimitService : public KeyedService,
                               public history::HistoryServiceObserver {
 public:
  explicit ImpressionLimitService(PrefService* pref_service,
                                  history::HistoryService* history_service);

  ImpressionLimitService(const ImpressionLimitService&) = delete;
  ImpressionLimitService& operator=(const ImpressionLimitService&) = delete;

  ~ImpressionLimitService() override;

  // Logs a magic stack impression for a card generated using the |url| in
  // the preference called |pref_name|.
  void LogImpressionForURL(const GURL& url, const std::string_view& pref_name);

  // Get Impression count for |url| stored in |pref_name|.
  std::optional<int> GetImpressionCount(const GURL& url,
                                        const std::string_view& pref_name);

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

  void LogImpressionForURLAtTime(const GURL& url,
                                 const std::string_view& pref_name,
                                 base::Time impression_time);

  void RemoveEntriesBeforeTime(const std::string_view& pref_name,
                               base::Time before_cutoff);

  const std::set<std::string_view> GetAllowListedPrefs();

  raw_ptr<history::HistoryService> history_service_;

  raw_ptr<PrefService> pref_service_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_IMPRESSION_LIMITS_IMPRESSION_LIMIT_SERVICE_H_
