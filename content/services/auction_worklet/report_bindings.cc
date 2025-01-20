// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/report_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8-value.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"

namespace auction_worklet {

ReportBindings::ReportBindings(AuctionV8Helper* v8_helper,
                               AuctionV8Logger* v8_logger,
                               bool queue_report_aggregate_win_allowed)
    : v8_helper_(v8_helper),
      v8_logger_(v8_logger),
      queue_report_aggregate_win_allowed_(queue_report_aggregate_win_allowed) {}

ReportBindings::~ReportBindings() = default;

void ReportBindings::AttachToContext(v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(context, &ReportBindings::SendReportTo, v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("sendReportTo"),
            v8_function)
      .Check();
  if (queue_report_aggregate_win_allowed_ &&
      base::FeatureList::IsEnabled(
          blink::features::kFledgePrivateModelTraining)) {
    v8::Local<v8::Function> v8_queue_aggregate_function =
        v8::Function::New(context, &ReportBindings::QueueReportAggregateWin,
                          v8_this)
            .ToLocalChecked();
    context->Global()
        ->Set(context,
              v8_helper_->CreateStringFromLiteral("queueReportAggregateWin"),
              v8_queue_aggregate_function)
        .Check();
  }
}

void ReportBindings::Reset() {
  report_url_ = std::nullopt;
  modeling_signals_config_ = std::nullopt;
  already_called_ = false;
  queue_already_called_ = false;
}

void ReportBindings::SendReportTo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ReportBindings* bindings =
      static_cast<ReportBindings*>(v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "sendReportTo(): ", &args,
                               /*min_required_args=*/1);
  std::string url_string;
  if (!args_converter.ConvertArg(0, "url", url_string)) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    // Note that we do not set `already_called_` here since in spec-land the
    // call did not actually happen.
    return;
  }

  if (bindings->already_called_) {
    bindings->report_url_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendReportTo may be called at most once")));
    return;
  }

  bindings->already_called_ = true;

  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendReportTo must be passed a valid HTTPS url")));
    return;
  }

  // There's no spec for max URL length, so don't throw in that case. Instead,
  // leave the report URL empty and display a warning.
  if (url.spec().size() > url::kMaxURLChars) {
    // Don't print out full URL in this case, since it will fill the entire
    // console.
    bindings->v8_logger_->LogConsoleWarning(
        base::StringPrintf("sendReportTo passed URL of length %" PRIuS
                           " but accepts URLs of at most length %" PRIuS ".",
                           url.spec().size(), url::kMaxURLChars));
    return;
  }

  bindings->report_url_ = url;
}

void ReportBindings::QueueReportAggregateWin(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ReportBindings* bindings =
      static_cast<ReportBindings*>(v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "queueReportAggregateWin() arguments: ", &args,
                               /*min_required_args=*/1);
  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }
  // `modeling_signals_config` is a nested JavaScript object.
  // `args_dict_converter` accesses the outer object to retrieve
  // "modelingSignalsConfig" and `config_dict_converter` accesses'
  // its properties.
  v8::Local<v8::Value> config_v8;
  ModelingSignalsConfig config;

  DictConverter args_dict_converter(
      v8_helper, time_limit_scope,
      "queueReportAggregateWin() argument 0: ", args[0]);
  if (args_dict_converter.is_failed()) {
    args_dict_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }
  args_dict_converter.GetRequired("modelingSignalsConfig", config_v8);

  DictConverter config_dict_converter(
      v8_helper, time_limit_scope,
      "queueReportAggregateWin() `modelingSignalsConfig` field: ", config_v8);

  std::string aggregation_coordinator_origin_string;
  config_dict_converter.GetRequired("aggregationCoordinatorOrigin",
                                    aggregation_coordinator_origin_string);
  std::string destination_url_string;
  config_dict_converter.GetRequired("destination", destination_url_string);
  config_dict_converter.GetRequired("payloadLength", config.payload_length);

  if (config_dict_converter.is_failed()) {
    config_dict_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }
  // Semantic Checks
  if (bindings->queue_already_called_) {
    bindings->modeling_signals_config_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "queueReportAggregateWin() may be called at most once")));
    return;
  }

  GURL aggregation_coordinator_origin_url(
      aggregation_coordinator_origin_string);
  if (!aggregation_coordinator_origin_url.is_valid() ||
      !aggregation_coordinator_origin_url.SchemeIs(url::kHttpsScheme)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "modelingSignalsConfig's aggregationCoordinatorOrigin must be "
            "passed a valid HTTPS url")));
    return;
  }
  // There's no spec for max URL length, so don't throw in that case. Instead,
  // leave the report URL empty and display a warning.
  if (aggregation_coordinator_origin_url.spec().size() > url::kMaxURLChars) {
    // Don't print out full URL in this case, since it will fill the entire
    // console.
    bindings->v8_logger_->LogConsoleWarning(base::StringPrintf(
        "modelingSignalsConfig's aggregationCoordinatorOrigin is a URL of "
        "length %" PRIuS " but accepts URLs of at most length %" PRIuS ".",
        aggregation_coordinator_origin_url.spec().size(), url::kMaxURLChars));
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "modelingSignalsConfig's aggregationCoordinatorOrigin exceeds the "
            "maximum URL length")));
    return;
  }
  config.aggregation_coordinator_origin =
      std::move(aggregation_coordinator_origin_url);

  GURL destination_url(destination_url_string);
  if (!destination_url.is_valid() ||
      !destination_url.SchemeIs(url::kHttpsScheme)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "modelingSignalsConfig's destination must be passed a valid HTTPS "
            "url")));
    return;
  }

  // There's no spec for max URL length, so don't throw in that case. Instead,
  // leave the report URL empty and display a warning.
  if (destination_url.spec().size() > url::kMaxURLChars) {
    // Don't print out full URL in this case, since it will fill the entire
    // console.
    bindings->v8_logger_->LogConsoleWarning(base::StringPrintf(
        "modelingSignalsConfig's destination is a URL of "
        "length %" PRIuS " but accepts URLs of at most length %" PRIuS ".",
        destination_url.spec().size(), url::kMaxURLChars));
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "modelingSignalsConfig's destination exceeds the "
            "maximum URL length")));
    return;
  }
  config.destination = std::move(destination_url);

  bindings->modeling_signals_config_ = std::move(config);
  bindings->queue_already_called_ = true;
}

}  // namespace auction_worklet
