// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_

#import <Foundation/Foundation.h>

@class FollowSiteInfo;
@class FollowWebPageURLs;
@class FollowedWebChannel;
class Browser;

// FollowProvider allows getting and setting the following status of web
// channels, and setting the event delegate to handle any updates from the
// backend.
class FollowProvider {
 public:
  FollowProvider() = default;
  virtual ~FollowProvider() = default;

  FollowProvider(const FollowProvider&) = delete;
  FollowProvider& operator=(const FollowProvider&) = delete;

  // TODO(crbug.com/1296745): Remove old API.
  // Returns true if the website with |followSiteInfo| has been followed.
  virtual bool GetFollowStatus(FollowSiteInfo* followSiteInfo);

  // Returns true if the web channel with |followWebPageURLs| has been followed.
  virtual bool GetFollowStatus(FollowWebPageURLs* followWebPageURLs);

  // Returns a list of followed web channels.
  virtual NSArray<FollowedWebChannel*>* GetFollowedWebChannels();

  // TODO(crbug.com/1296745): Remove old API.
  // Updates the following status of |site| to |state|.
  virtual void UpdateFollowStatus(FollowSiteInfo* site, bool state);

  // Updates the following status of the web channel associated with
  // |followWebPageURLs|. The web channel is followed if |followStatus| is true.
  virtual void UpdateFollowStatus(FollowWebPageURLs* followWebPageURLs,
                                  bool followStatus);

  // Sets the follow event delegate to discover feed with |browser|.
  // This method must be called before any follow action needs to be handled.
  virtual void SetFollowEventDelegate(Browser* browser);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_
