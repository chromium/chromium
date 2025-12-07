// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class Browser;
enum class DiscoverFeedEligibility;
@protocol DiscoverFeedVisibilityObserver;
@protocol DiscoverFeedVisibilityProvider;

/// Browser agent that handles the of discover feed visibility.
class DiscoverFeedVisibilityBrowserAgent
    : public BrowserUserData<DiscoverFeedVisibilityBrowserAgent> {
 public:
  /// Not copyable or moveable
  DiscoverFeedVisibilityBrowserAgent(
      const DiscoverFeedVisibilityBrowserAgent&) = delete;
  DiscoverFeedVisibilityBrowserAgent& operator=(
      const DiscoverFeedVisibilityBrowserAgent&) = delete;
  ~DiscoverFeedVisibilityBrowserAgent() override;

  /// Enabled state accessors.
  bool IsEnabled();
  void SetEnabled(bool enabled);

  /// Eligibility getter.
  DiscoverFeedEligibility GetEligibility();

  /// Convenience method that returns whether the feed should be visible.
  bool ShouldBeVisible();

  /// Adds or removes `observer` to/from a list of observers that gets notified
  /// of Discover feed eligibility or visibility changes.
  void AddObserver(id<DiscoverFeedVisibilityObserver> observer);
  void RemoveObserver(id<DiscoverFeedVisibilityObserver> observer);

 private:
  friend class BrowserUserData<DiscoverFeedVisibilityBrowserAgent>;
  explicit DiscoverFeedVisibilityBrowserAgent(Browser* browser);

  /// The object that handles visibility updates. Lazy loaded.
  id<DiscoverFeedVisibilityProvider> visibility_provider_;
  /// Method to create and/or retrieve the visibility provider for lazy loading
  /// purpose.
  id<DiscoverFeedVisibilityProvider> GetVisibilityProvider();
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_BROWSER_AGENT_H_
