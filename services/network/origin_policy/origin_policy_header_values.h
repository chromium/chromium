// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_HEADER_VALUES_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_HEADER_VALUES_H_

#include <string>

namespace network {
// Represents a parsed `Sec-Origin-Policy` header.
// Spec: https://wicg.github.io/origin-policy/#origin-policy-header
struct OriginPolicyHeaderValues {
  // The policy version that is parsed from the `policy=` parameter.
  std::string policy_version;
  // The report group to send reports to if an error occurs. Uses the
  // reporting API. Parsed from the `report-to=` parameter.
  std::string report_to;
  // The raw header that was used to populate this struct. Added to the report
  // that is send in case on an origin policy error.
  std::string raw_header;
};
}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_HEADER_VALUES_H_
