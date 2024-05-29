// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_CAPABILITIES_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_CAPABILITIES_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/supervised_user_capabilities.h"

// Implement this protocol and pass your implementation into an
// SupervisedUserCapabilitiesObserving object to receive
// SupervisedUserCapabilitiesObserver callbacks in Objective-C.
@protocol SupervisedUserCapabilitiesObserving <NSObject>

- (void)onIsSubjectToParentalControlsCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState;

@end

namespace supervised_user {

class SupervisedUserCapabilitiesObserverBridge
    : public SupervisedUserCapabilitiesObserver {
 public:
  SupervisedUserCapabilitiesObserverBridge(
      signin::IdentityManager* identity_manager,
      id<SupervisedUserCapabilitiesObserving> observing);

  SupervisedUserCapabilitiesObserverBridge(
      const SupervisedUserCapabilitiesObserverBridge&) = delete;
  SupervisedUserCapabilitiesObserverBridge& operator=(
      const SupervisedUserCapabilitiesObserverBridge&) = delete;

  void OnIsSubjectToParentalControlsCapabilityChanged(
      CapabilityUpdateState capability_update_state) override;

 private:
  // Observing object to be bridged.
  __weak id<SupervisedUserCapabilitiesObserving> observing_;
};

}  // namespace supervised_user

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_CAPABILITIES_OBSERVER_BRIDGE_H_
