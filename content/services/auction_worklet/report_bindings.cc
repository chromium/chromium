// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/report_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"

namespace auction_worklet {

ReportBindings::ReportBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

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
}

void ReportBindings::Reset() {
  report_url_ = absl::nullopt;
  already_called_ = false;
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

  bindings->report_url_ = url;
}

}  // namespace auction_worklet
