// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom-shared.h"
#include "url/origin.h"

namespace network {
struct CrossOriginEmbedderPolicy;

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

  bool IsEqualExcludingOrigin(const CrossOriginOpenerPolicy& other) const;

  mojom::CrossOriginOpenerPolicyValue value =
      mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  std::optional<std::string> reporting_endpoint;
  mojom::CrossOriginOpenerPolicyValue report_only_value =
      mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  std::optional<std::string> report_only_reporting_endpoint;
  mojom::CrossOriginOpenerPolicyValue soap_by_default_value =
      mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;

  // The origin that sets this policy.  May stay nullopt until sandbox flags
  // are ready so we can calculate the sandboxed origin.
  std::optional<url::Origin> origin;
};

COMPONENT_EXPORT(NETWORK_CPP_BASE)
bool IsAccessFromCoopPage(mojom::CoopAccessReportType);

COMPONENT_EXPORT(NETWORK_CPP_BASE)
const char* CoopAccessReportTypeToString(mojom::CoopAccessReportType type);

COMPONENT_EXPORT(NETWORK_CPP_BASE)
void AugmentCoopWithCoep(CrossOriginOpenerPolicy* coop,
                         const CrossOriginEmbedderPolicy& coep);

COMPONENT_EXPORT(NETWORK_CPP_BASE)
bool IsRelatedToCoopRestrictProperties(
    mojom::CrossOriginOpenerPolicyValue value);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_H_
