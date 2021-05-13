// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_HEADER_VALUES_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_HEADER_VALUES_H_

#include <string>
#include "base/component_export.h"
#include "base/optional.h"

namespace network {

// Represents a value from the Origin-Policy header's allowed=() list.
//
// Values can be a string, or the special tokens `latest` or `null`.
class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyAllowedValue final {
 public:
  ~OriginPolicyAllowedValue();
  OriginPolicyAllowedValue(const OriginPolicyAllowedValue&);

  // The caller is expected to a string that is a valid origin policy ID:
  // https://wicg.github.io/origin-policy/#valid-origin-policy-id
  static const OriginPolicyAllowedValue FromString(const std::string&);
  static const OriginPolicyAllowedValue Null();
  static const OriginPolicyAllowedValue Latest();

  bool operator==(const OriginPolicyAllowedValue& other) const {
    return state_ == other.state_ && string_ == other.string_;
  }
  bool operator!=(const OriginPolicyAllowedValue& other) const {
    return !(*this == other);
  }

  bool is_latest() const {
    return state_ == OriginPolicyAllowedValue::State::kLatestToken;
  }
  bool is_null() const {
    return state_ == OriginPolicyAllowedValue::State::kNullToken;
  }
  bool is_string() const {
    return state_ == OriginPolicyAllowedValue::State::kString;
  }

  const std::string& string() const {
    DCHECK(is_string());
    DCHECK(string_.has_value());
    return *string_;
  }

 private:
  enum class State { kString, kLatestToken, kNullToken };

  OriginPolicyAllowedValue(State, const base::Optional<std::string>&);

  State state_;
  base::Optional<std::string> string_;
};

// Represents a value from the Origin-Policy header's preferred= entry.
//
// Values can be a string, or the special token `latest-from-network`.
class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyPreferredValue final {
 public:
  ~OriginPolicyPreferredValue();
  OriginPolicyPreferredValue(const OriginPolicyPreferredValue&);

  // The caller is expected to a string that is a valid origin policy ID:
  // https://wicg.github.io/origin-policy/#valid-origin-policy-id
  static const OriginPolicyPreferredValue FromString(const std::string&);
  static const OriginPolicyPreferredValue LatestFromNetwork();

  bool operator==(const OriginPolicyPreferredValue& other) const {
    return string_ == other.string_;
  }
  bool operator!=(const OriginPolicyPreferredValue& other) const {
    return !(*this == other);
  }

  bool is_latest_from_network() const { return !string_.has_value(); }
  bool is_string() const { return string_.has_value(); }

  const std::string& string() const {
    DCHECK(is_string());
    return *string_;
  }

 private:
  explicit OriginPolicyPreferredValue(const base::Optional<std::string>&);

  // If string_ is base::nullopt, then this is latest-from-network. This is a
  // small optimization compared to the State enum used for
  // OriginPolicyAllowedValue.
  base::Optional<std::string> string_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_HEADER_VALUES_H_
