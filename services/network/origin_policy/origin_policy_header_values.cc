// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_header_values.h"
#include "base/check.h"

namespace network {

OriginPolicyAllowedValue::OriginPolicyAllowedValue(
    OriginPolicyAllowedValue::State state,
    const absl::optional<std::string>& string)
    : state_(state), string_(string) {}

OriginPolicyAllowedValue::~OriginPolicyAllowedValue() = default;
OriginPolicyAllowedValue::OriginPolicyAllowedValue(
    const OriginPolicyAllowedValue&) = default;

// static
const OriginPolicyAllowedValue OriginPolicyAllowedValue::FromString(
    const std::string& string) {
  DCHECK(!string.empty());
  return OriginPolicyAllowedValue(OriginPolicyAllowedValue::State::kString,
                                  string);
}

// static
const OriginPolicyAllowedValue OriginPolicyAllowedValue::Latest() {
  return OriginPolicyAllowedValue(OriginPolicyAllowedValue::State::kLatestToken,
                                  std::string());
}

// static
const OriginPolicyAllowedValue OriginPolicyAllowedValue::Null() {
  return OriginPolicyAllowedValue(OriginPolicyAllowedValue::State::kNullToken,
                                  std::string());
}

OriginPolicyPreferredValue::OriginPolicyPreferredValue(
    const absl::optional<std::string>& string)
    : string_(string) {}

OriginPolicyPreferredValue::~OriginPolicyPreferredValue() = default;
OriginPolicyPreferredValue::OriginPolicyPreferredValue(
    const OriginPolicyPreferredValue&) = default;

// static
const OriginPolicyPreferredValue OriginPolicyPreferredValue::FromString(
    const std::string& string) {
  DCHECK(!string.empty());
  return OriginPolicyPreferredValue(string);
}

// static
const OriginPolicyPreferredValue
OriginPolicyPreferredValue::LatestFromNetwork() {
  return OriginPolicyPreferredValue(absl::nullopt);
}

}  // namespace network
