// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_OBSERVER_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ios/chrome/browser/follow/model/followed_web_site.h"

// Observer class notified about FollowService changes.
class FollowServiceObserver : public base::CheckedObserver {
 public:
  FollowServiceObserver();

  FollowServiceObserver(const FollowServiceObserver&) = delete;
  FollowServiceObserver& operator=(const FollowServiceObserver&) = delete;

  ~FollowServiceObserver() override;

  // Invoked when a website is followed.
  virtual void OnWebSiteFollowed(FollowedWebSite* web_site) = 0;

  // Invoked when a website is unfollowed.
  virtual void OnWebSiteUnfollowed(FollowedWebSite* web_site) = 0;

  // Invoked when followed websites are loaded.
  virtual void OnFollowedWebSitesLoaded() = 0;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_OBSERVER_H_
