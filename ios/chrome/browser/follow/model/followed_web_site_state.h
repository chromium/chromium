// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOWED_WEB_SITE_STATE_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOWED_WEB_SITE_STATE_H_

#import <Foundation/Foundation.h>

// Enum representing the followed website states.
typedef NS_ENUM(NSInteger, FollowedWebSiteState) {
  FollowedWebSiteStateStateUnknown = 0,
  FollowedWebSiteStateStateInactive,
  FollowedWebSiteStateStateActive,
  FollowedWebSiteStateStateAwaitingContent,
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOWED_WEB_SITE_STATE_H_
