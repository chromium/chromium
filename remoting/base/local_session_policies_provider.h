// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_LOCAL_SESSION_POLICIES_PROVIDER_H_
#define REMOTING_BASE_LOCAL_SESSION_POLICIES_PROVIDER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "remoting/base/session_policies.h"

namespace remoting {

// Class that provides the local SessionPolicies.
class LocalSessionPoliciesProvider final {
 public:
  using LocalPoliciesChangedCallbackList =
      base::RepeatingCallbackList<void(const SessionPolicies&)>;
  using LocalPoliciesChangedCallback =
      LocalPoliciesChangedCallbackList::CallbackType;

  LocalSessionPoliciesProvider();
  ~LocalSessionPoliciesProvider();

  LocalSessionPoliciesProvider(const LocalSessionPoliciesProvider&) = delete;
  LocalSessionPoliciesProvider& operator=(const LocalSessionPoliciesProvider&) =
      delete;

  // Calls `callback` whenever the local policies are changed.
  // This is marked `const` to allow a holder of a const reference or pointer of
  // `this` to add callbacks but not change the local policies.
  base::CallbackListSubscription AddLocalPoliciesChangedCallback(
      LocalPoliciesChangedCallback callback) const;

  const SessionPolicies& get_local_policies() const { return local_policies_; }

  // Sets the local policies. If the new policies are equal to the previous
  // policies LocalPoliciesChanged callbacks will not be notified.
  void set_local_policies(const SessionPolicies& policies);

 private:
  SessionPolicies local_policies_;
  // Mutable to allow const access from `AddLocalPoliciesChangedCallback()`.
  mutable LocalPoliciesChangedCallbackList local_policies_changed_callbacks_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_LOCAL_SESSION_POLICIES_PROVIDER_H_
