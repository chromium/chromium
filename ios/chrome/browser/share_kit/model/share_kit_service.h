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
@class ShareKitDeleteConfiguration;
@class ShareKitFacePileConfiguration;
@class ShareKitJoinConfiguration;
@class ShareKitLeaveConfiguration;
@class ShareKitLookupGaiaIDConfiguration;
@class ShareKitManageConfiguration;
@class ShareKitReadGroupWithTokenConfiguration;
@class ShareKitReadGroupsConfiguration;
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

  // Ensures that the service is using the current primary account.
  virtual void PrimaryAccountChanged() = 0;

  // Cancels the current `sessionID` flow.
  virtual void CancelSession(NSString* session_id) = 0;

  // Initiates the share group flow for the given `config` and returns its
  // sessionID.
  virtual NSString* ShareTabGroup(ShareKitShareGroupConfiguration* config) = 0;

  // Initiates the flow to manage the group, using `config` and returns its
  // sessionID.
  virtual NSString* ManageTabGroup(ShareKitManageConfiguration* config) = 0;

  // Initiates the flow to join the group, using `config` and returns its
  // sessionID.
  virtual NSString* JoinTabGroup(ShareKitJoinConfiguration* config) = 0;

  // Returns a new FacePile view for the given `config`.
  virtual UIView* FacePileView(ShareKitFacePileConfiguration* config) = 0;

  // Reads the info for the groups passed in `config` and returns the result
  // through the config callback.
  virtual void ReadGroups(ShareKitReadGroupsConfiguration* config) = 0;

  // Reads the info for the group passed in `config` and returns the result
  // through the config callback.
  virtual void ReadGroupWithToken(
      ShareKitReadGroupWithTokenConfiguration* config) = 0;

  // Leaves the group passed in `config` and returns the result through the
  // config callback.
  virtual void LeaveGroup(ShareKitLeaveConfiguration* config) = 0;

  // Deletes the group passed in `config` and returns the result through the
  // config callback.
  virtual void DeleteGroup(ShareKitDeleteConfiguration* config) = 0;

  // Looks up the gaia ID associated with the email from `config` and returns
  // the result through the config callback.
  virtual void LookupGaiaIdByEmail(
      ShareKitLookupGaiaIDConfiguration* config) = 0;

  // Returns a wrapper object of the avatar image for the avatar URL passed in
  // `config`.
  virtual id<ShareKitAvatarPrimitive> AvatarImage(
      ShareKitAvatarConfiguration* config) = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_
