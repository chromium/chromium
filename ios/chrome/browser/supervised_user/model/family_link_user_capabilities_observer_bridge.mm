// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/family_link_user_capabilities_observer_bridge.h"

namespace supervised_user {

FamilyLinkUserCapabilitiesObserverBridge::
    FamilyLinkUserCapabilitiesObserverBridge(
        signin::IdentityManager* identity_manager,
        id<FamilyLinkUserCapabilitiesObserving> observing)
    : FamilyLinkUserCapabilitiesObserver(identity_manager),
      observing_(observing) {}

void FamilyLinkUserCapabilitiesObserverBridge::
    OnIsSubjectToParentalControlsCapabilityChanged(
        supervised_user::CapabilityUpdateState capability_update_state) {
  if ([observing_ respondsToSelector:@selector
                  (onIsSubjectToParentalControlsCapabilityChanged:)]) {
    [observing_
        onIsSubjectToParentalControlsCapabilityChanged:capability_update_state];
  }
}

void FamilyLinkUserCapabilitiesObserverBridge::
    OnCanFetchFamilyMemberInfoCapabilityChanged(
        supervised_user::CapabilityUpdateState capability_update_state) {
  if ([observing_ respondsToSelector:@selector
                  (onCanFetchFamilyMemberInfoCapabilityChanged:)]) {
    [observing_
        onCanFetchFamilyMemberInfoCapabilityChanged:capability_update_state];
  }
}

}  // namespace supervised_user
