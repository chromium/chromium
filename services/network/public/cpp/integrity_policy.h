// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/integrity_policy.mojom-shared.h"

namespace network {

// This implements a data structure holding information from a parsed
// `Integrity-Policy` or `Integrity-Policy-Report-Only` header.
//
// This struct is needed as we can't use the mojom generated struct directly
// from the blink public API, given that we cannot include .mojo.h there due to
// DEPS rules.
struct COMPONENT_EXPORT(NETWORK_CPP_INTEGRITY_POLICY) IntegrityPolicy {
  IntegrityPolicy();
  ~IntegrityPolicy();

  IntegrityPolicy(IntegrityPolicy&&);
  IntegrityPolicy& operator=(IntegrityPolicy&&);

  IntegrityPolicy(const IntegrityPolicy&);
  IntegrityPolicy& operator=(const IntegrityPolicy&);

  bool operator==(const IntegrityPolicy&) const;

  std::vector<mojom::IntegrityPolicy_Destination> blocked_destinations;
  std::vector<mojom::IntegrityPolicy_Source> sources;
  std::vector<std::string> endpoints;
  std::vector<std::string> parsing_errors;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_H_
