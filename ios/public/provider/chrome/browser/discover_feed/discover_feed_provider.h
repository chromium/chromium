// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class Browser;
@class DiscoverFeedConfiguration;
@class DiscoverFeedViewControllerConfiguration;
@class FeedMetricsRecorder;

// DiscoverFeedProvider allows embedders to provide functionality for a Discover
// Feed.
class DiscoverFeedProvider {
 public:
  // Observer class for discover feed events.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called whenever the FeedProvider Model has changed. At this point all
    // existing Feed ViewControllers are stale and need to be refreshed.
    virtual void OnDiscoverFeedModelRecreated() = 0;
  };

  DiscoverFeedProvider() = default;
  virtual ~DiscoverFeedProvider() = default;

  DiscoverFeedProvider(const DiscoverFeedProvider&) = delete;
  DiscoverFeedProvider& operator=(const DiscoverFeedProvider&) = delete;

  // Starts the Feed service using |discover_config| which contains various
  // configs for the Feed.
  virtual void StartFeedService(DiscoverFeedConfiguration* discover_config);
  // Stops the Feed, which will disconnect all of its services and clear the
  // models.
  virtual void StopFeedService();
  // TODO(crbug.com/1277504): Remove when cleaned out of downstream.
  virtual void StartFeed(DiscoverFeedConfiguration* discover_config);
  virtual void StopFeed();
  // Creates models for all enabled feed types.
  virtual void CreateFeedModels();
  // Clears all existing feed models.
  virtual void ClearFeedModels();
  // Returns true if the Discover Feed is enabled.
  virtual bool IsDiscoverFeedEnabled();
  // Returns the FeedMetricsRecorder to be used by the feed. There only exists a
  // single instance of the metrics recorder per browser state.
  virtual FeedMetricsRecorder* GetFeedMetricsRecorder();
  // Returns the Discover Feed ViewController with a custom
  // DiscoverFeedViewControllerConfiguration.
  virtual UIViewController* NewDiscoverFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration);
  // Returns the Following Feed ViewController with a custom
  // DiscoverFeedViewControllerConfiguration.
  virtual UIViewController* NewFollowingFeedViewControllerWithConfiguration(
      DiscoverFeedViewControllerConfiguration* configuration);
  // Removes the Discover |feedViewController|. It should be called whenever
  // |feedViewController| will no longer be used.
  virtual void RemoveFeedViewController(UIViewController* feedViewController);
  // Updates the feed's theme to match the user's theme (light/dark).
  virtual void UpdateTheme();
  // Refreshes the Discover Feed if needed. The provider decides if a refresh is
  // needed or not.
  virtual void RefreshFeedIfNeeded();
  // Refreshes the Discover Feed. Once the Feed model is refreshed it will
  // update all ViewControllers returned by NewFeedViewController.
  virtual void RefreshFeed();
  // Updates the Feed for an account change e.g. Signing In/Out.
  virtual void UpdateFeedForAccountChange();
  // Methods to register or remove observers.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_PROVIDER_H_
