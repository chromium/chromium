// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace network {

CrossOriginOpenerPolicy::CrossOriginOpenerPolicy() = default;
CrossOriginOpenerPolicy::CrossOriginOpenerPolicy(
    const CrossOriginOpenerPolicy& src) = default;
CrossOriginOpenerPolicy::CrossOriginOpenerPolicy(
    CrossOriginOpenerPolicy&& src) = default;
CrossOriginOpenerPolicy::~CrossOriginOpenerPolicy() = default;

CrossOriginOpenerPolicy& CrossOriginOpenerPolicy::operator=(
    const CrossOriginOpenerPolicy& src) = default;
CrossOriginOpenerPolicy& CrossOriginOpenerPolicy::operator=(
    CrossOriginOpenerPolicy&& src) = default;
bool CrossOriginOpenerPolicy::operator==(
    const CrossOriginOpenerPolicy& other) const {
  return IsEqualExcludingOrigin(other) && origin == other.origin;
}

bool CrossOriginOpenerPolicy::IsEqualExcludingOrigin(
    const CrossOriginOpenerPolicy& other) const {
  return value == other.value &&
         reporting_endpoint == other.reporting_endpoint &&
         report_only_value == other.report_only_value &&
         report_only_reporting_endpoint ==
             other.report_only_reporting_endpoint &&
         soap_by_default_value == other.soap_by_default_value;
}

bool IsAccessFromCoopPage(mojom::CoopAccessReportType type) {
  switch (type) {
    case mojom::CoopAccessReportType::kAccessFromCoopPageToOpener:
    case mojom::CoopAccessReportType::kAccessFromCoopPageToOpenee:
    case mojom::CoopAccessReportType::kAccessFromCoopPageToOther:
      return true;
    case mojom::CoopAccessReportType::kAccessToCoopPageFromOpener:
    case mojom::CoopAccessReportType::kAccessToCoopPageFromOpenee:
    case mojom::CoopAccessReportType::kAccessToCoopPageFromOther:
      return false;
  }
}

const char* CoopAccessReportTypeToString(mojom::CoopAccessReportType type) {
  switch (type) {
    case network::mojom::CoopAccessReportType::kAccessFromCoopPageToOpener:
      return "access-from-coop-page-to-opener";
    case network::mojom::CoopAccessReportType::kAccessFromCoopPageToOpenee:
      return "access-from-coop-page-to-openee";
    case network::mojom::CoopAccessReportType::kAccessFromCoopPageToOther:
      return "access-from-coop-page-to-other";
    case network::mojom::CoopAccessReportType::kAccessToCoopPageFromOpener:
      return "access-to-coop-page-from-opener";
    case network::mojom::CoopAccessReportType::kAccessToCoopPageFromOpenee:
      return "access-to-coop-page-from-openee";
    case network::mojom::CoopAccessReportType::kAccessToCoopPageFromOther:
      return "access-to-coop-page-from-other";
  }
}

// [spec]: https://html.spec.whatwg.org/C/#obtain-coop
void AugmentCoopWithCoep(CrossOriginOpenerPolicy* coop,
                         const CrossOriginEmbedderPolicy& coep) {
  // [spec]: 4.1.2. If coep's value is compatible with cross-origin isolation,
  //                then set policy's value to "same-origin-plus-COEP".

  // "COOP: same-origin" case.
  if (coop->value == mojom::CrossOriginOpenerPolicyValue::kSameOrigin &&
      CompatibleWithCrossOriginIsolated(coep.value)) {
    coop->value = mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
    coop->soap_by_default_value =
        mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  }

  // "COOP: restrict-properties" case.
  if (coop->value == mojom::CrossOriginOpenerPolicyValue::kRestrictProperties &&
      CompatibleWithCrossOriginIsolated(coep.value)) {
    coop->value =
        mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
    coop->soap_by_default_value =
        mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
  }

  // COOP-Report-Only:
  //
  // [spec]: 6.1.2. If coep's value is compatible with cross-origin isolation or
  // coep's report-only value is compatible with cross-origin isolation, then
  // set policy's report-only value to "same-origin-plus-COEP".
  if (coop->report_only_value ==
          mojom::CrossOriginOpenerPolicyValue::kSameOrigin &&
      (CompatibleWithCrossOriginIsolated(coep.value) ||
       CompatibleWithCrossOriginIsolated(coep.report_only_value))) {
    coop->report_only_value =
        mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  }

  // COOP: restrict-properties report-only case.
  if (coop->report_only_value ==
          mojom::CrossOriginOpenerPolicyValue::kRestrictProperties &&
      (CompatibleWithCrossOriginIsolated(coep.value) ||
       CompatibleWithCrossOriginIsolated(coep.report_only_value))) {
    coop->report_only_value =
        mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
  }
}

bool IsRelatedToCoopRestrictProperties(
    mojom::CrossOriginOpenerPolicyValue value) {
  return value == mojom::CrossOriginOpenerPolicyValue::kRestrictProperties ||
         value ==
             mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
}

}  // namespace network
