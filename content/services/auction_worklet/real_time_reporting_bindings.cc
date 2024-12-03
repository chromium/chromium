// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/real_time_reporting_bindings.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/common/content_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/blink/public/common/features.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

// Attempts to parse the elements of `args`, and add the constructed
// contribution to `contributions_out`. Throws an exception on most failures.
void ParseAndCollectContribution(
    AuctionV8Helper* v8_helper,
    AuctionV8Logger* v8_logger,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>&
        contributions_out) {
  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(
      v8_helper, time_limit_scope,
      "realTimeReporting.contributeToHistogram(): ", &args,
      /*min_required_args=*/1);

  v8::Local<v8::Value> contribution_val;
  args_converter.ConvertArg(0, "contribution", contribution_val);

  int32_t idl_bucket;
  double idl_priority_weight;
  std::optional<int32_t> idl_latency_threshold;

  if (args_converter.is_success()) {
    // arg 0 is:
    // dictionary RealTimeContribution {
    //   required long bucket;
    //   required double priorityWeight;
    //   long latencyThreshold;
    // };
    DictConverter contribution_converter(
        v8_helper, time_limit_scope,
        "realTimeReporting.contributeToHistogram() 'contribution' argument: ",
        contribution_val);

    // Note that this happens in lexicographic order of field names, to match
    // WebIDL behavior.
    contribution_converter.GetRequired("bucket", idl_bucket);
    contribution_converter.GetOptional("latencyThreshold",
                                       idl_latency_threshold);
    contribution_converter.GetRequired("priorityWeight", idl_priority_weight);
    args_converter.SetStatus(contribution_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  // [0, features::kFledgeRealTimeReportingNumBuckets) are supported buckets
  // for the API, while platform contribution buckets start from
  // features::kFledgeRealTimeReportingNumBuckets.
  if (idl_bucket < 0 ||
      idl_bucket >= blink::features::kFledgeRealTimeReportingNumBuckets.Get()) {
    // Don't throw, to be forward compatible.
    return;
  }

  if (idl_priority_weight <= 0) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateUtf8String("priorityWeight must be a positive Number")
            .ToLocalChecked()));
    return;
  }

  contributions_out.push_back(
      auction_worklet::mojom::RealTimeReportingContribution::New(
          idl_bucket, idl_priority_weight, idl_latency_threshold));
}

}  // namespace

RealTimeReportingBindings::RealTimeReportingBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

RealTimeReportingBindings::~RealTimeReportingBindings() = default;

void RealTimeReportingBindings::AttachToContext(
    v8::Local<v8::Context> context) {
  // Don't enable real time reporting in Mode A/B traffic.
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeRealTimeReporting) ||
      base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return;
  }

  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::External> v8_this = v8::External::New(isolate, this);
  v8::Local<v8::Object> real_time_reporting = v8::Object::New(isolate);

  v8::Local<v8::FunctionTemplate> real_time_histogram_template =
      v8::FunctionTemplate::New(
          isolate, &RealTimeReportingBindings::ContributeToHistogram, v8_this);

  real_time_reporting
      ->Set(context,
            v8_helper_->CreateStringFromLiteral("contributeToHistogram"),
            real_time_histogram_template->GetFunction(context).ToLocalChecked())
      .Check();

  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("realTimeReporting"),
            real_time_reporting)
      .Check();
}

void RealTimeReportingBindings::Reset() {
  real_time_reporting_contributions_.clear();
}

void RealTimeReportingBindings::ContributeToHistogram(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  RealTimeReportingBindings* bindings = static_cast<RealTimeReportingBindings*>(
      v8::External::Cast(*args.Data())->Value());
  ParseAndCollectContribution(bindings->v8_helper_.get(),
                              bindings->v8_logger_.get(), args,
                              bindings->real_time_reporting_contributions_);
}

}  // namespace auction_worklet
