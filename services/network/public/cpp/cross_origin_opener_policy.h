// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_H_

#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom-shared.h"

namespace network {

// This corresponds to network::mojom::CrossOriginOpenerPolicy.
// See the comments there.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) CrossOriginOpenerPolicy final {
  CrossOriginOpenerPolicy();
  ~CrossOriginOpenerPolicy();
  CrossOriginOpenerPolicy(const CrossOriginOpenerPolicy&);
  CrossOriginOpenerPolicy(CrossOriginOpenerPolicy&&);
  CrossOriginOpenerPolicy& operator=(const CrossOriginOpenerPolicy&);
  CrossOriginOpenerPolicy& operator=(CrossOriginOpenerPolicy&&);
  bool operator==(const CrossOriginOpenerPolicy&) const;

  mojom::CrossOriginOpenerPolicyValue value =
      mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  base::Optional<std::string> reporting_endpoint;
  mojom::CrossOriginOpenerPolicyValue report_only_value =
      mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  base::Optional<std::string> report_only_reporting_endpoint;
};

COMPONENT_EXPORT(NETWORK_CPP_BASE)
bool IsAccessFromCoopPage(mojom::CoopAccessReportType);

COMPONENT_EXPORT(NETWORK_CPP_BASE)
const char* CoopAccessReportTypeToString(mojom::CoopAccessReportType type);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_H_
