// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_

#import <Foundation/Foundation.h>

@class FollowSiteInfo;

// FollowProvider provides and updates the following status of websites and
// provides information related to these.
class FollowProvider {
 public:
  FollowProvider() = default;
  virtual ~FollowProvider() = default;

  FollowProvider(const FollowProvider&) = delete;
  FollowProvider& operator=(const FollowProvider&) = delete;

  // Returns YES if the website with |followSiteInfo| has been followed.
  virtual bool GetFollowStatus(FollowSiteInfo* followSiteInfo);

  // Returns a list of followed websites.
  virtual NSMutableArray<FollowSiteInfo*>* GetFollowedChannels();

  // Updates the following status of |sites| to |status|.
  virtual void updateFollowStatus(NSMutableArray<FollowSiteInfo*>* sites,
                                  bool status);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_
