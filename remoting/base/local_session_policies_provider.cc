// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/local_session_policies_provider.h"

#include "remoting/base/session_policies.h"

namespace remoting {

LocalSessionPoliciesProvider::LocalSessionPoliciesProvider() = default;
LocalSessionPoliciesProvider::~LocalSessionPoliciesProvider() = default;

base::CallbackListSubscription
LocalSessionPoliciesProvider::AddLocalPoliciesChangedCallback(
    LocalPoliciesChangedCallback callback) const {
  return local_policies_changed_callbacks_.Add(std::move(callback));
}

void LocalSessionPoliciesProvider::set_local_policies(
    const SessionPolicies& policies) {
  if (policies == local_policies_) {
    return;
  }
  local_policies_ = policies;
  local_policies_changed_callbacks_.Notify(local_policies_);
}

}  // namespace remoting
