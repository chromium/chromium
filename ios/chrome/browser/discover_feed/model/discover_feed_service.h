// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_SERVICE_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_SERVICE_H_

#import <UIKit/UIKit.h>

#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/discover_feed/model/discover_feed_observer.h"
#include "ios/chrome/browser/discover_feed/model/discover_feed_refresher.h"
#include "ios/chrome/browser/discover_feed/model/discover_feed_view_controller_configuration.h"
#include "ios/chrome/browser/discover_feed/model/feed_constants.h"
#include "ios/chrome/browser/discover_feed/model/feed_model_configuration.h"

@class FeedMetricsRecorder;

// A browser-context keyed service that is used to keep the Discover Feed data
// up to date.
class DiscoverFeedService : public DiscoverFeedRefresher, public KeyedService {
 public:
  DiscoverFeedService();
  ~DiscoverFeedService() override;

  // Creates models for all enabled feed types.
  virtual void CreateFeedModels() = 0;

  // Creates a single feed model based on the given model configuration.
  virtual void CreateFeedModel(FeedModelConfiguration* feed_model_config) = 0;

  // Clears all existing feed models.
  virtual void ClearFeedModels() = 0;

  // Sets the Following feed sorting and refreshes the model to display it.
  virtual void SetFollowingFeedSortType(FollowingFeedSortType sort_type) = 0;

  // Sets whether the feed is currently being shown on the Start Surface.
  virtual void SetIsShownOnStartSurface(bool shown_on_start_surface);

  // Returns the FeedMetricsRecorder to be used by the feed. There only exists a
  // single instance of the metrics recorder per profile.
  virtual FeedMetricsRecorder* GetFeedMetricsRecorder() = 0;

  // Returns the Discover Feed ViewController with a custom
  // DiscoverFeedViewControllerConfiguration.
  virtual UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) = 0;

  // Returns the Following Feed ViewController with a custom
  // DiscoverFeedViewControllerConfiguration.
  virtual UIViewController* NewFollowingFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration) = 0;

  // Removes the Discover `feed_view_controller`. It should be called whenever
  // `feed_view_controller` will no longer be used.
  virtual void RemoveFeedViewController(
      UIViewController* feed_view_controller) = 0;

  // Updates the feed's theme to match the user's theme (light/dark).
  virtual void UpdateTheme() = 0;

  // Returns whether the Following feed model has unseen content.
  virtual BOOL GetFollowingFeedHasUnseenContent() = 0;

  // Informs the service that the Following content has been seen.
  virtual void SetFollowingFeedContentSeen() = 0;

  // Informs the service that Browsing History data was cleread by the user.
  virtual void BrowsingHistoryCleared();

  // Methods to register or remove observers.
  void AddObserver(DiscoverFeedObserver* observer);
  void RemoveObserver(DiscoverFeedObserver* observer);

 protected:
  void NotifyDiscoverFeedModelRecreated();

 private:
  // List of DiscoverFeedObservers.
  base::ObserverList<DiscoverFeedObserver, true> observer_list_;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_SERVICE_H_
