// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_utils.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

namespace content::private_aggregation {

std::string GetReportingPath(PrivateAggregationBudgetKey::Api api,
                             bool is_immediate_debug_report) {
  // TODO(alexmt): Consider updating or making a FeatureParam.
  static constexpr char kSharedReportingPathPrefix[] =
      "/.well-known/private-aggregation/";
  static constexpr char kDebugReportingPathInfix[] = "debug/";
  static constexpr char kProtectedAudienceReportingPathSuffix[] =
      "report-protected-audience";
  static constexpr char kSharedStorageReportingPathSuffix[] =
      "report-shared-storage";

  base::StringPiece api_suffix;
  switch (api) {
    case PrivateAggregationBudgetKey::Api::kProtectedAudience:
      api_suffix = kProtectedAudienceReportingPathSuffix;
      break;
    case PrivateAggregationBudgetKey::Api::kSharedStorage:
      api_suffix = kSharedStorageReportingPathSuffix;
      break;
  }

  return base::StrCat(
      {kSharedReportingPathPrefix,
       is_immediate_debug_report ? kDebugReportingPathInfix : "", api_suffix});
}

std::string GetApiIdentifier(PrivateAggregationBudgetKey::Api api) {
  switch (api) {
    case PrivateAggregationBudgetKey::Api::kProtectedAudience:
      return "protected-audience";
    case PrivateAggregationBudgetKey::Api::kSharedStorage:
      return "shared-storage";
  }
}

}  // namespace content::private_aggregation
