// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_HISTORY_OBSERVER_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_HISTORY_OBSERVER_SERVICE_H_

#import "base/memory/scoped_refptr.h"
#import "base/scoped_observation.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/history_service_observer.h"
#import "components/keyed_service/core/keyed_service.h"

class HostContentSettingsMap;

// Profile-keyed service that observes `history::HistoryService` deletions and
// revokes corresponding entries in `SuspiciousSiteWarningAllowlist`.
class SuspiciousSiteHistoryObserverService
    : public KeyedService,
      public history::HistoryServiceObserver {
 public:
  // Constructs the service to observe `history_service` deletions and update
  // `host_content_settings_map`.
  SuspiciousSiteHistoryObserverService(
      history::HistoryService* history_service,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);

  SuspiciousSiteHistoryObserverService(
      const SuspiciousSiteHistoryObserverService&) = delete;
  SuspiciousSiteHistoryObserverService& operator=(
      const SuspiciousSiteHistoryObserverService&) = delete;

  ~SuspiciousSiteHistoryObserverService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // history::HistoryServiceObserver implementation.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_HISTORY_OBSERVER_SERVICE_H_
