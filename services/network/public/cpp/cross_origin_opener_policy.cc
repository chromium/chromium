// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy.h"

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
  return value == other.value &&
         reporting_endpoint == other.reporting_endpoint &&
         report_only_value == other.report_only_value &&
         report_only_reporting_endpoint == other.report_only_reporting_endpoint;
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

}  // namespace network
