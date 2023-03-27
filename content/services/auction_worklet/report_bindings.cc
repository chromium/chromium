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
#include "gin/converter.h"
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
  exception_thrown_ = false;
}

void ReportBindings::SendReportTo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ReportBindings* bindings =
      static_cast<ReportBindings*>(v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  std::string url_string;
  if (args.Length() < 1 || args[0].IsEmpty() ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[0], &url_string)) {
    bindings->exception_thrown_ = true;
    bindings->report_url_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendReportTo requires 1 string parameter")));
    return;
  }

  if (bindings->exception_thrown_ || bindings->report_url_) {
    bindings->exception_thrown_ = true;
    bindings->report_url_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendReportTo may be called at most once")));
    return;
  }

  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    bindings->exception_thrown_ = true;
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendReportTo must be passed a valid HTTPS url")));
    return;
  }

  bindings->report_url_ = url;
}

}  // namespace auction_worklet
