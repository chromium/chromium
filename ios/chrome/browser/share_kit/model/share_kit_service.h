// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"

@protocol ShareKitAvatarPrimitive;
@class ShareKitAvatarConfiguration;
@class ShareKitFacePileConfiguration;
@class ShareKitJoinConfiguration;
@class ShareKitManageConfiguration;
@class ShareKitReadConfiguration;
@class ShareKitShareGroupConfiguration;

// Service for ShareKit, allowing to manage tab groups sharing.
class ShareKitService : public KeyedService {
 public:
  ShareKitService();
  ShareKitService(const ShareKitService&) = delete;
  ShareKitService& operator=(const ShareKitService&) = delete;
  ~ShareKitService() override;

  // Whether the service is supported. This value does not change during the
  // execution of the application.
  virtual bool IsSupported() const = 0;

  // Initiates the share group flow for the given `config`.
  virtual void ShareGroup(ShareKitShareGroupConfiguration* config) = 0;

  // Initiates the flow to manage the group, using `config`.
  virtual void ManageGroup(ShareKitManageConfiguration* config) = 0;

  // Initiates the flow to join the group, using `config`.
  virtual void JoinGroup(ShareKitJoinConfiguration* config) = 0;

  // Returns a new FacePile view controller for the given `config`.
  virtual UIViewController* FacePile(ShareKitFacePileConfiguration* config) = 0;

  // Reads the info for the groups passed in `config` and returns the result
  // through the config callback.
  virtual void ReadGroups(ShareKitReadConfiguration* config);

  // Returns a wrapper object of the avatar image for the avatar URL passed in
  // `config`.
  virtual id<ShareKitAvatarPrimitive> AvatarImage(
      ShareKitAvatarConfiguration* config);
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_
