// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/follow/follow_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool FollowProvider::GetFollowStatus(FollowWebPageURLs* follow_web_page_urls) {
  return false;
}

NSArray<FollowedWebChannel*>* FollowProvider::GetFollowedWebChannels() {
  return nil;
}

void FollowProvider::UpdateFollowStatus(FollowWebPageURLs* follow_web_page_urls,
                                        bool follow_status) {}

void FollowProvider::SetFollowEventDelegate(Browser* browser) {}

void FollowProvider::AddFollowManagementUIUpdater(
    id<FollowManagementUIUpdater> follow_management_ui_updater) {}

void FollowProvider::RemoveFollowManagementUIUpdater(
    id<FollowManagementUIUpdater> follow_management_ui_updater) {}
