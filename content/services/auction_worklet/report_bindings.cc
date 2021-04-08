// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/report_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace auction_worklet {

ReportBindings::ReportBindings(AuctionV8Helper* v8_helper,
                               v8::Local<v8::ObjectTemplate> global_template)
    : v8_helper_(v8_helper) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::FunctionTemplate> v8_template = v8::FunctionTemplate::New(
      v8_helper_->isolate(), &ReportBindings::SendReportTo, v8_this);
  v8_template->RemovePrototype();
  global_template->Set(v8_helper_->CreateStringFromLiteral("sendReportTo"),
                       v8_template);
}

ReportBindings::~ReportBindings() = default;

void ReportBindings::SendReportTo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ReportBindings* bindings =
      static_cast<ReportBindings*>(v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  std::string url_string;
  if (args.Length() < 1 || args[0].IsEmpty() ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[0], &url_string)) {
    bindings->exception_thrown_ = true;
    bindings->report_url_ = GURL();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "SendReportTo requires 1 string parameter.")));
    return;
  }

  if (bindings->exception_thrown_ || bindings->report_url_.is_valid()) {
    bindings->exception_thrown_ = true;
    bindings->report_url_ = GURL();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "SendReportTo may be called at most once.")));
  }

  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    bindings->exception_thrown_ = true;
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "SendReportTo must be passed a valid HTTPS url.")));
    return;
  }

  bindings->report_url_ = url;
}

}  // namespace auction_worklet
