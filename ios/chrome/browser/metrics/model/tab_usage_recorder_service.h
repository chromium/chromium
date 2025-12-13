// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#include "ios/chrome/browser/sessions/model/session_restoration_service.h"
#include "ios/chrome/browser/shared/model/browser/browser.h"
#include "ios/chrome/browser/shared/model/browser/browser_list.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"

// Service that observes all tabs for a Profile and reports metrics
// about them (total number of tabs open per types, ...). Does not
// expose any public API as it merely listen to events are records
// metrics once created until it is destroyed.
class TabUsageRecorderService final : public KeyedService,
                                      public BrowserListObserver,
                                      public SessionRestorationObserver {
 public:
  TabUsageRecorderService(
      BrowserList* browser_list,
      SessionRestorationService* session_restoration_service);

  ~TabUsageRecorderService() final;

  // Records the session metrics.
  void RecordSessionMetrics();

  // KeyedService:
  void Shutdown() override;

  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* list, Browser* browser) final;
  void OnBrowserRemoved(const BrowserList* list, Browser* browser) final;

  // SessionRestorationObserver:
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

 private:
  class Helper;

  // Invoked when the off-the-record profile is destroyed.
  void OnOTRProfileDestroyed();

  // Observation of the BrowserList.
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  // Observation of the SessionRestorationService.
  base::ScopedObservation<SessionRestorationService, SessionRestorationObserver>
      session_restoration_observation_{this};

  // Stores Helper instances used to observe Browsers per type.
  std::map<Browser::Type, std::unique_ptr<Helper>> helpers_;

  // Subscription for listening to the off-the-record profile destruction.
  base::CallbackListSubscription otr_profile_destruction_subscription_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_SERVICE_H_
