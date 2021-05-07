// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_

#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ChromeBrowserState;
@class DiscoverFeedMetricsRecorder;

// A browser-context keyed service that is used to keep the Discover Feed data
// up to date.
class DiscoverFeedService : public KeyedService,
                            public signin::IdentityManager::Observer {
 public:
  // Initializes the service.
  explicit DiscoverFeedService(ChromeBrowserState* browser_state);
  ~DiscoverFeedService() override;

  // Returns the FeedMetricsRecorder to be used by the Feed, a single instance
  // of DiscoverFeedMetricsRecorder needs to be used per BrowserState.
  DiscoverFeedMetricsRecorder* GetDiscoverFeedMetricsRecorder();

  // KeyedService:
  void Shutdown() override;

 private:
  // IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // Helper to track registration as an signin::IdentityManager::Observer.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Metrics recorder for the DiscoverFeed.
  __strong DiscoverFeedMetricsRecorder* discover_feed_metrics_recorder_ = nil;

  DISALLOW_COPY_AND_ASSIGN(DiscoverFeedService);
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_
