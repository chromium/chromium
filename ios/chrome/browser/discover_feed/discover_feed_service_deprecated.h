// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_DEPRECATED_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_DEPRECATED_H_

#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/discover_feed/discover_feed_service.h"
#include "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#include "ios/public/provider/chrome/browser/discover_feed/discover_feed_view_controller_configuration.h"

class AuthenticationService;
class PrefService;

// An implementation of DiscoverFeedService that uses the deprecated
// API of DiscoverFeedProvider.
class DiscoverFeedServiceDeprecated : public DiscoverFeedService,
                                      public signin::IdentityManager::Observer,
                                      public DiscoverFeedProvider::Observer {
 public:
  // Initializes the service.
  DiscoverFeedServiceDeprecated(PrefService* pref_service,
                                AuthenticationService* authentication_service,
                                signin::IdentityManager* identity_manager);

  ~DiscoverFeedServiceDeprecated() override;

  // KeyedService implementation:
  void Shutdown() override;

  // DiscoverFeedService implementation:
  void CreateFeedModels() override;
  void ClearFeedModels() override;
  FeedMetricsRecorder* GetFeedMetricsRecorder() override;
  UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) override;
  UIViewController* NewFollowingFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) override;
  void RemoveFeedViewController(
      UIViewController* feed_view_controller) override;
  void UpdateTheme() override;
  void RefreshFeedIfNeeded() override;
  void RefreshFeed() override;

  // IdentityManager::Observer implementation:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // DiscoverFeedProvider::Observer implementation:
  void OnDiscoverFeedModelRecreated() override;

 private:
  // Helper to track registration as an signin::IdentityManager::Observer.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Helper to track registration as an DiscoverFeedProvider::Observer.
  base::ScopedObservation<DiscoverFeedProvider, DiscoverFeedProvider::Observer>
      discover_feed_provider_observation_{this};

  // Metrics recorder for the DiscoverFeed.
  __strong FeedMetricsRecorder* feed_metrics_recorder_ = nil;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_DEPRECATED_H_
