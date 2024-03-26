// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

@class STWebHistory;

// ScreenTimeHistoryDeleter is responsible for deleting ScreenTime history when
// Chrome history is deleted.
class ScreenTimeHistoryDeleter : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  explicit ScreenTimeHistoryDeleter(history::HistoryService* history_service);

  ScreenTimeHistoryDeleter(const ScreenTimeHistoryDeleter&) = delete;
  ScreenTimeHistoryDeleter& operator=(const ScreenTimeHistoryDeleter&) = delete;

  ~ScreenTimeHistoryDeleter() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  raw_ptr<history::HistoryService> history_service_;
  STWebHistory* screen_time_history_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_H_
