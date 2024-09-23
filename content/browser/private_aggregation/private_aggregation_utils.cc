// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_utils.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"

namespace content::private_aggregation {

std::string GetReportingPath(PrivateAggregationCallerApi api,
                             bool is_immediate_debug_report) {
  // TODO(alexmt): Consider updating or making a FeatureParam.
  static constexpr char kSharedReportingPathPrefix[] =
      "/.well-known/private-aggregation/";
  static constexpr char kDebugReportingPathInfix[] = "debug/";
  static constexpr char kProtectedAudienceReportingPathSuffix[] =
      "report-protected-audience";
  static constexpr char kSharedStorageReportingPathSuffix[] =
      "report-shared-storage";

  std::string_view api_suffix;
  switch (api) {
    case PrivateAggregationCallerApi::kProtectedAudience:
      api_suffix = kProtectedAudienceReportingPathSuffix;
      break;
    case PrivateAggregationCallerApi::kSharedStorage:
      api_suffix = kSharedStorageReportingPathSuffix;
      break;
  }

  return base::StrCat(
      {kSharedReportingPathPrefix,
       is_immediate_debug_report ? kDebugReportingPathInfix : "", api_suffix});
}

std::string GetApiIdentifier(PrivateAggregationCallerApi api) {
  switch (api) {
    case PrivateAggregationCallerApi::kProtectedAudience:
      return "protected-audience";
    case PrivateAggregationCallerApi::kSharedStorage:
      return "shared-storage";
  }
}

}  // namespace content::private_aggregation
