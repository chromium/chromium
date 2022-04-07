// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/follow/follow_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1296745): Remove old API.
bool FollowProvider::GetFollowStatus(FollowSiteInfo* followSiteInfo) {
  return false;
}

bool FollowProvider::GetFollowStatus(FollowWebPageURLs* followWebPageURLs) {
  return false;
}

NSArray<FollowedWebChannel*>* FollowProvider::GetFollowedWebChannels() {
  return nil;
}

// TODO(crbug.com/1296745): Remove old API.
void FollowProvider::UpdateFollowStatus(FollowSiteInfo* site, bool state) {}

void FollowProvider::UpdateFollowStatus(FollowWebPageURLs* followWebPageURLs,
                                        bool state) {}

void FollowProvider::SetFollowEventDelegate(Browser* browser) {}
