// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_REQUEST_HEADERS_INTERNAL_H_
#define SERVICES_NETWORK_ATTRIBUTION_REQUEST_HEADERS_INTERNAL_H_

#include <stdint.h>

#include <string>

#include "services/network/public/mojom/attribution.mojom-forward.h"

namespace network {

// Options controlling greasing during serialization of the
// Attribution-Reporting-Eligible and Attribution-Reportnig-Support headers,
// which contain structured dictionaries.
struct AttributionReportingHeaderGreaseOptions {
  // Where to apply a grease.
  enum class GreaseContext : uint8_t {
    // The grease is not applied.
    kNone = 0,
    // The grease is added as a top-level key prefixed with "not-".
    kKey = 1,
    // The grease is set as the token-type value for an existing key.
    kValue = 2,
    // The grease is set as a parameter name for an existing key.
    kParamName = 3,
  };

  // Note: This function needs exactly 8 bits. If the size of this structure changes,
  // `uint8_t` should be changed to a larger type.
  static AttributionReportingHeaderGreaseOptions FromBits(uint8_t bits);

  // Whether to reverse the list of dictionary keys.
  bool reverse = false;

  // Whether to swap the two greases before applying.
  bool swap_greases = false;

  // Where to apply the greases.
  GreaseContext context1 = GreaseContext::kNone;
  GreaseContext context2 = GreaseContext::kNone;

  // Whether to apply each grease to the first or last existing key.
  bool use_front1 = false;
  bool use_front2 = false;
};

// Must not be called with `mojom::AttributionReportingEligibility::kUnset`.
std::string SerializeAttributionReportingEligibleHeader(
    mojom::AttributionReportingEligibility,
    const AttributionReportingHeaderGreaseOptions&);

// Returns the value to be set for the `Attribution-Reporting-Support` request
// header.
std::string GetAttributionSupportHeader(
    mojom::AttributionSupport,
    const AttributionReportingHeaderGreaseOptions&);

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_REQUEST_HEADERS_INTERNAL_H_
