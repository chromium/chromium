// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"

#import "ios/chrome/test/providers/discover_feed/test_discover_feed_service.h"

namespace ios {
namespace provider {

std::unique_ptr<DiscoverFeedService> CreateDiscoverFeedService(
    DiscoverFeedConfiguration* configuration) {
  return std::make_unique<TestDiscoverFeedService>();
}

}  // namespace provider
}  // namespace ios
