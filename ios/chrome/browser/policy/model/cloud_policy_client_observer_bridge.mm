// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/cloud_policy_client_observer_bridge.h"

CloudPolicyClientObserverBridge::CloudPolicyClientObserverBridge(
    id<CloudPolicyClientObserver> observer_delegate,
    policy::CloudPolicyClient* cloud_policy_client_observer)
    : observer_(observer_delegate) {
  DCHECK(observer_);

  scoped_observation_.Observe(cloud_policy_client_observer);
}

CloudPolicyClientObserverBridge::~CloudPolicyClientObserverBridge() {}

void CloudPolicyClientObserverBridge::OnPolicyFetched(
    policy::CloudPolicyClient* client) {
  [observer_ cloudPolicyWasFetched:client];
}

void CloudPolicyClientObserverBridge::OnClientError(
    policy::CloudPolicyClient* client) {
  [observer_ cloudPolicyDidError:client];
}

void CloudPolicyClientObserverBridge::OnRegistrationStateChanged(
    policy::CloudPolicyClient* client) {
  [observer_ cloudPolicyRegistrationChanged:client];
}
