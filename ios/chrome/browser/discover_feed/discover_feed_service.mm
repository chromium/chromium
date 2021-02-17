// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/discover_feed/discover_feed_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/discover_feed_metrics_recorder.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_configuration.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedService::DiscoverFeedService(ChromeBrowserState* browser_state) {
  discover_feed_provider_ =
      ios::GetChromeBrowserProvider()->GetDiscoverFeedProvider();
  identity_manager_ = IdentityManagerFactory::GetForBrowserState(browser_state);
  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }

  discover_feed_metrics_recorder_ = [[DiscoverFeedMetricsRecorder alloc] init];

  DiscoverFeedConfiguration* discover_config =
      [[DiscoverFeedConfiguration alloc] init];
  discover_config.browserState = browser_state;
  discover_config.metricsRecorder = discover_feed_metrics_recorder_;
  discover_feed_provider_->StartFeed(discover_config);
}

DiscoverFeedService::~DiscoverFeedService() {}

DiscoverFeedMetricsRecorder*
DiscoverFeedService::GetDiscoverFeedMetricsRecorder() {
  return discover_feed_metrics_recorder_;
}

void DiscoverFeedService::Shutdown() {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
}

void DiscoverFeedService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  discover_feed_provider_->UpdateFeedForAccountChange();
}
