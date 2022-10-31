// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/private_aggregation_bindings.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/worklet_utils/private_aggregation_utils.h"
#include "gin/arguments.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

PrivateAggregationBindings::PrivateAggregationBindings(
    AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

PrivateAggregationBindings::~PrivateAggregationBindings() = default;

void PrivateAggregationBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  if (!base::FeatureList::IsEnabled(content::kPrivateAggregationApi) ||
      !content::kPrivateAggregationApiEnabledInFledge.Get()) {
    return;
  }

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  v8::Local<v8::ObjectTemplate> private_aggregation_template =
      v8::ObjectTemplate::New(v8_helper_->isolate());

  v8::Local<v8::FunctionTemplate> send_histogram_report_template =
      v8::FunctionTemplate::New(
          v8_helper_->isolate(),
          &PrivateAggregationBindings::SendHistogramReport, v8_this);
  send_histogram_report_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("sendHistogramReport"),
      send_histogram_report_template);

  v8::Local<v8::FunctionTemplate> enable_debug_mode_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &PrivateAggregationBindings::EnableDebugMode,
                                v8_this);
  enable_debug_mode_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("enableDebugMode"),
      enable_debug_mode_template);

  global_template->Set(
      v8_helper_->CreateStringFromLiteral("privateAggregation"),
      private_aggregation_template);
}

void PrivateAggregationBindings::Reset() {
  private_aggregation_contributions_.clear();
  debug_mode_details_.is_enabled = false;
  debug_mode_details_.debug_key = nullptr;
}

std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
PrivateAggregationBindings::TakePrivateAggregationRequests() {
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests;

  requests.reserve(private_aggregation_contributions_.size());
  base::ranges::transform(
      private_aggregation_contributions_, std::back_inserter(requests),
      [this](content::mojom::AggregatableReportHistogramContributionPtr&
                 contribution) {
        return auction_worklet::mojom::PrivateAggregationRequest::New(
            std::move(contribution),
            // TODO(alexmt): consider allowing this to be set
            content::mojom::AggregationServiceMode::kDefault,
            debug_mode_details_.Clone());
      });
  private_aggregation_contributions_.clear();

  return requests;
}

void PrivateAggregationBindings::SendHistogramReport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());

  content::mojom::AggregatableReportHistogramContributionPtr contribution =
      worklet_utils::ParseSendHistogramReportArguments(gin::Arguments(args));
  if (contribution.is_null()) {
    // Indicates an exception was thrown.
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      std::move(contribution));
}

void PrivateAggregationBindings::EnableDebugMode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());

  worklet_utils::ParseAndApplyEnableDebugModeArguments(
      gin::Arguments(args), bindings->debug_mode_details_);
}

}  // namespace auction_worklet
