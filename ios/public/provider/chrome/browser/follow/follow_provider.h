// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_

#import <Foundation/Foundation.h>

@class FollowWebPageURLs;
@class FollowedWebChannel;
@protocol FollowManagementUIUpdater;
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

  // Returns true if the web channel with |follow_web_page_urls| has been
  // followed.
  virtual bool GetFollowStatus(FollowWebPageURLs* follow_web_page_urls);

  // Returns a list of followed web channels.
  virtual NSArray<FollowedWebChannel*>* GetFollowedWebChannels();

  // Updates the following status of the web channel associated with
  // |follow_web_page_urls|. The web channel is followed if |followStatus| is
  // true.
  virtual void UpdateFollowStatus(FollowWebPageURLs* follow_web_page_urls,
                                  bool follow_status);

  // Sets the follow event delegate to discover feed with |browser|.
  // This method must be called before any follow action needs to be handled.
  virtual void SetFollowEventDelegate(Browser* browser);

  // Adds a |follow_management_ui_updater| that handles changes of follow
  // management UI.
  virtual void AddFollowManagementUIUpdater(
      id<FollowManagementUIUpdater> follow_management_ui_updater);

  // Removes a |follow_management_ui_updater| that handles changes of follow
  // management UI.
  virtual void RemoveFollowManagementUIUpdater(
      id<FollowManagementUIUpdater> follow_management_ui_updater);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_FOLLOW_FOLLOW_PROVIDER_H_
