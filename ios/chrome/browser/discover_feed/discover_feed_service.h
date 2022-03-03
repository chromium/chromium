// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_

#import <UIKit/UIKit.h>

#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/discover_feed/discover_feed_observer.h"
#include "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#include "ios/public/provider/chrome/browser/discover_feed/discover_feed_view_controller_configuration.h"

class AuthenticationService;
@class FeedMetricsRecorder;
class PrefService;

// A browser-context keyed service that is used to keep the Discover Feed data
// up to date.
class DiscoverFeedService : public KeyedService,
                            public signin::IdentityManager::Observer,
                            public DiscoverFeedProvider::Observer {
 public:
  // Initializes the service.
  DiscoverFeedService(PrefService* pref_service,
                      AuthenticationService* authentication_service,
                      signin::IdentityManager* identity_manager);

  DiscoverFeedService(const DiscoverFeedService&) = delete;
  DiscoverFeedService& operator=(const DiscoverFeedService&) = delete;

  ~DiscoverFeedService() override;

  // KeyedService:
  void Shutdown() override;

  // Creates models for all enabled feed types.
  void CreateFeedModels();

  // Clears all existing feed models.
  void ClearFeedModels();

  // Returns the FeedMetricsRecorder to be used by the feed. There only exists a
  // single instance of the metrics recorder per browser state.
  FeedMetricsRecorder* GetFeedMetricsRecorder();

  // Returns the Discover Feed ViewController with a custom
  // DiscoverFeedViewControllerConfiguration.
  UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration);

  // Returns the Following Feed ViewController with a custom
  // DiscoverFeedViewControllerConfiguration.
  UIViewController* NewFollowingFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration);

  // Removes the Discover |feed_view_controller|. It should be called whenever
  // |feed_view_controller| will no longer be used.
  void RemoveFeedViewController(UIViewController* feed_view_controller);

  // Updates the feed's theme to match the user's theme (light/dark).
  void UpdateTheme();

  // Refreshes the Discover Feed if needed. The provider decides if a refresh is
  // needed or not.
  void RefreshFeedIfNeeded();

  // Refreshes the Discover Feed. Once the Feed model is refreshed it will
  // update all ViewControllers returned by NewFeedViewController.
  void RefreshFeed();

  // Methods to register or remove observers.
  void AddObserver(DiscoverFeedObserver* observer);
  void RemoveObserver(DiscoverFeedObserver* observer);

 private:
  // IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // DiscoverFeedProvider::Observer.
  void OnDiscoverFeedModelRecreated() override;

  // Helper to track registration as an signin::IdentityManager::Observer.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Helper to track registration as an DiscoverFeedProvider::Observer.
  base::ScopedObservation<DiscoverFeedProvider, DiscoverFeedProvider::Observer>
      discover_feed_provider_observation_{this};

  // List of DiscoverFeedObservers.
  base::ObserverList<DiscoverFeedObserver, true> observer_list_;

  // Metrics recorder for the DiscoverFeed.
  __strong FeedMetricsRecorder* feed_metrics_recorder_ = nil;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_SERVICE_H_
