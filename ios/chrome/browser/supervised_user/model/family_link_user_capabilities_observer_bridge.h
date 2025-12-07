// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_FAMILY_LINK_USER_CAPABILITIES_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_FAMILY_LINK_USER_CAPABILITIES_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/family_link_user_capabilities.h"

// Implement this protocol and pass your implementation into an
// FamilyLinkUserCapabilitiesObserving object to receive
// FamilyLinkUserCapabilitiesObserver callbacks in Objective-C.
@protocol FamilyLinkUserCapabilitiesObserving <NSObject>

@optional
- (void)onIsSubjectToParentalControlsCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState;

@optional
- (void)onCanFetchFamilyMemberInfoCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState;

@end

namespace supervised_user {

class FamilyLinkUserCapabilitiesObserverBridge
    : public FamilyLinkUserCapabilitiesObserver {
 public:
  FamilyLinkUserCapabilitiesObserverBridge(
      signin::IdentityManager* identity_manager,
      id<FamilyLinkUserCapabilitiesObserving> observing);

  FamilyLinkUserCapabilitiesObserverBridge(
      const FamilyLinkUserCapabilitiesObserverBridge&) = delete;
  FamilyLinkUserCapabilitiesObserverBridge& operator=(
      const FamilyLinkUserCapabilitiesObserverBridge&) = delete;

  void OnIsSubjectToParentalControlsCapabilityChanged(
      CapabilityUpdateState capability_update_state) override;

  void OnCanFetchFamilyMemberInfoCapabilityChanged(
      CapabilityUpdateState capability_update_state) override;

 private:
  // Observing object to be bridged.
  __weak id<FamilyLinkUserCapabilitiesObserving> observing_;
};

}  // namespace supervised_user

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_FAMILY_LINK_USER_CAPABILITIES_OBSERVER_BRIDGE_H_
