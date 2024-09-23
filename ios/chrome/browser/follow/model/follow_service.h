// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_H_

#import <Foundation/Foundation.h>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/follow/model/followed_web_site.h"
#include "ios/chrome/browser/follow/model/web_page_urls.h"

class FollowServiceObserver;

// Represents the result of an operation (follow or unfollow).
enum class FollowResult {
  Success,
  Failure,
};

// Represents the source of the follow request.
enum class FollowSource {
  OverflowMenu,
  PopupMenu,
  Management,
  Retry,
  Undo,
};

// FollowService allows interaction with websites. It allows querying for
// the channels that are followed, follow new channels, unfollow channels and
// to register observer to be notified of change to the list of channels.
class FollowService : public KeyedService {
 public:
  // Callbacks invoked when an operation completes.
  using ResultCallback =
      base::OnceCallback<void(FollowResult result, FollowedWebSite* web_site)>;

  // Returns if a followed website corresponds to `web_page_urls`.
  virtual bool IsWebSiteFollowed(WebPageURLs* web_page_urls) = 0;

  // If a recommended website corresponds to `web_page_urls`, returns
  // the URL identifier for the website. Returns nil otherwise.
  virtual NSURL* GetRecommendedSiteURL(WebPageURLs* web_page_urls) = 0;

  // Returns a list of all followed websites.
  virtual NSArray<FollowedWebSite*>* GetFollowedWebSites() = 0;

  // Load a list of all followed websites.
  virtual void LoadFollowedWebSites() = 0;

  // Follows the website associated with `web_page_urls` and invokes
  // `callback` with the operation result. The callback will not be
  // invoked if the website is already followed.
  virtual void FollowWebSite(WebPageURLs* web_page_urls,
                             FollowSource source,
                             ResultCallback callback) = 0;

  // Unfollows the website associated with `web_page_urls` and invokes
  // `callback` with the operation result. The callback will not be
  // invoked if the website is not yet followed.
  virtual void UnfollowWebSite(WebPageURLs* web_page_urls,
                               FollowSource source,
                               ResultCallback callback) = 0;

  // Adds/Removes `observer`.
  virtual void AddObserver(FollowServiceObserver* observer) = 0;
  virtual void RemoveObserver(FollowServiceObserver* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_H_
