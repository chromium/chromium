// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_POLICY_CLIENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_POLICY_CLIENT_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include "base/scoped_observation.h"

// Objective-C protocol mirroring
// policy::CloudPolicyClient::Observer.
@protocol CloudPolicyClientObserver <NSObject>
- (void)cloudPolicyWasFetched:(policy::CloudPolicyClient*)client;
- (void)cloudPolicyDidError:(policy::CloudPolicyClient*)client;
- (void)cloudPolicyRegistrationChanged:(policy::CloudPolicyClient*)client;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class CloudPolicyClientObserverBridge
    : public policy::CloudPolicyClient::Observer {
 public:
  CloudPolicyClientObserverBridge(
      id<CloudPolicyClientObserver> observer_delegate,
      policy::CloudPolicyClient* cloud_policy_client_observer);
  CloudPolicyClientObserverBridge(const CloudPolicyClientObserverBridge&) =
      delete;
  CloudPolicyClientObserverBridge& operator=(
      const CloudPolicyClientObserverBridge&) = delete;
  ~CloudPolicyClientObserverBridge() override;

  // policy::CloudPolicyClient::Observer implementation.
  void OnPolicyFetched(policy::CloudPolicyClient* client) override;
  void OnClientError(policy::CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(policy::CloudPolicyClient* client) override;

 private:
  __weak id<CloudPolicyClientObserver> observer_;
  base::ScopedObservation<policy::CloudPolicyClient,
                          policy::CloudPolicyClient::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLOUD_POLICY_CLIENT_OBSERVER_BRIDGE_H_
