// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_EXPERIMENTS_TRACKER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_EXPERIMENTS_TRACKER_H_

#import "base/memory/raw_ptr.h"
#import "components/feed/core/v2/public/ios/prefs.h"

class PrefService;

// Handles storing and retrieving experiments in prefs and registering them as
// synthetic field trials.
class DiscoverFeedExperimentsTracker final {
 public:
  explicit DiscoverFeedExperimentsTracker(PrefService* pref_service);
  ~DiscoverFeedExperimentsTracker();
  DiscoverFeedExperimentsTracker(const DiscoverFeedExperimentsTracker&) =
      delete;
  DiscoverFeedExperimentsTracker& operator=(
      const DiscoverFeedExperimentsTracker&) = delete;

  // Stores experiments in prefs and registers them as synthetic field trials.
  void UpdateExperiments(const feed::Experiments& experiments);

 private:
  // Registers experiments as synthetic field trials.
  void RegisterExperiments(const feed::Experiments& experiments);

  // Pref service used to store and retrieve experiments.
  raw_ptr<PrefService> pref_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_EXPERIMENTS_TRACKER_H_
