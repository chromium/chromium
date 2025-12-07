// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_POLICIES_FROM_DICT_H_
#define REMOTING_HOST_SESSION_POLICIES_FROM_DICT_H_

#include <optional>

#include "base/values.h"
#include "remoting/base/session_policies.h"

namespace remoting {

// Create SessionPolicies from the policy dictionary. nullopt is returned if
// the dictionary contains invalid entries.
// Note: When using PolicyWatcher, please pass the dictionary returned by
// GetPlatformPolicies(). The dictionary passed to the PolicyUpdatedCallback is
// a delta, but this function takes the full policy dictionary.
std::optional<SessionPolicies> SessionPoliciesFromDict(
    const base::Value::Dict& dict);

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_POLICIES_FROM_DICT_H_
