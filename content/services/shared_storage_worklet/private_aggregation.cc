// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/private_aggregation.h"

#include <string>
#include <utility>

#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/services/worklet_utils/private_aggregation_utils.h"
#include "gin/arguments.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "v8/include/v8-isolate.h"

namespace shared_storage_worklet {

PrivateAggregation::PrivateAggregation(
    mojom::SharedStorageWorkletServiceClient& client,
    bool private_aggregation_permissions_policy_allowed,
    content::mojom::PrivateAggregationHost& private_aggregation_host)
    : client_(client),
      private_aggregation_permissions_policy_allowed_(
          private_aggregation_permissions_policy_allowed),
      private_aggregation_host_(private_aggregation_host) {}

PrivateAggregation::~PrivateAggregation() = default;

gin::WrapperInfo PrivateAggregation::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder PrivateAggregation::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<PrivateAggregation>::GetObjectTemplateBuilder(isolate)
      .SetMethod("sendHistogramReport",
                 &PrivateAggregation::SendHistogramReport)
      .SetMethod("enableDebugMode", &PrivateAggregation::EnableDebugMode);
}

const char* PrivateAggregation::GetTypeName() {
  return "PrivateAggregation";
}

void PrivateAggregation::SendHistogramReport(gin::Arguments* args) {
  EnsureUseCountersAreRecorded();

  content::mojom::AggregatableReportHistogramContributionPtr contribution =
      worklet_utils::ParseSendHistogramReportArguments(
          *args, private_aggregation_permissions_policy_allowed_);
  if (contribution.is_null()) {
    // Indicates an exception was thrown.
    return;
  }

  std::vector<content::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(std::move(contribution));

  private_aggregation_host_->SendHistogramReport(
      std::move(contributions),
      // TODO(alexmt): consider allowing this to be set
      content::mojom::AggregationServiceMode::kDefault,
      debug_mode_details_.Clone());
}

void PrivateAggregation::EnableDebugMode(gin::Arguments* args) {
  EnsureUseCountersAreRecorded();

  worklet_utils::ParseAndApplyEnableDebugModeArguments(
      *args, private_aggregation_permissions_policy_allowed_,
      debug_mode_details_);
}

void PrivateAggregation::EnsureUseCountersAreRecorded() {
  if (!has_recorded_use_counters_) {
    has_recorded_use_counters_ = true;
    client_->RecordUseCounters(
        {blink::mojom::WebFeature::kPrivateAggregationApiAll,
         blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage});
  }
}

}  // namespace shared_storage_worklet
