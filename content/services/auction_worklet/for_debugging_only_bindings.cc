// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/for_debugging_only_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

ForDebuggingOnlyBindings::ForDebuggingOnlyBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}
ForDebuggingOnlyBindings::~ForDebuggingOnlyBindings() = default;

void ForDebuggingOnlyBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::External> v8_this = v8::External::New(isolate, this);
  v8::Local<v8::ObjectTemplate> debugging_template =
      v8::ObjectTemplate::New(isolate);

  v8::Local<v8::FunctionTemplate> loss_template = v8::FunctionTemplate::New(
      isolate, &ForDebuggingOnlyBindings::ReportAdAuctionLoss, v8_this);
  v8::Local<v8::FunctionTemplate> win_template = v8::FunctionTemplate::New(
      isolate, &ForDebuggingOnlyBindings::ReportAdAuctionWin, v8_this);
  // If runtime flag BiddingAndScoringDebugReportingAPI is not enabled,
  // forDebuggingOnly.reportAdAuctionLoss() and
  // forDebuggingOnly.reportAdAuctionWin() APIs will be disabled (do nothing).
  // They are still valid APIs doing nothing instead of causing Javascript
  // errors.
  if (!base::FeatureList::IsEnabled(
          blink::features::kBiddingAndScoringDebugReportingAPI)) {
    loss_template = v8::FunctionTemplate::New(isolate);
    win_template = v8::FunctionTemplate::New(isolate);
  }
  loss_template->RemovePrototype();
  debugging_template->Set(
      v8_helper_->CreateStringFromLiteral("reportAdAuctionLoss"),
      loss_template);

  win_template->RemovePrototype();
  debugging_template->Set(
      v8_helper_->CreateStringFromLiteral("reportAdAuctionWin"), win_template);

  global_template->Set(v8_helper_->CreateStringFromLiteral("forDebuggingOnly"),
                       debugging_template);
}

void ForDebuggingOnlyBindings::Reset() {
  loss_report_url_ = absl::nullopt;
  win_report_url_ = absl::nullopt;
}

void ForDebuggingOnlyBindings::ReportAdAuctionLoss(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ForDebuggingOnlyBindings* bindings = static_cast<ForDebuggingOnlyBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  std::string url_string;
  if (args.Length() < 1 || args[0].IsEmpty() ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[0], &url_string)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "reportAdAuctionLoss requires 1 string parameter")));
    return;
  }

  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "reportAdAuctionLoss must be passed a valid HTTPS url")));
    return;
  }
  bindings->loss_report_url_ = url;
}

void ForDebuggingOnlyBindings::ReportAdAuctionWin(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ForDebuggingOnlyBindings* bindings = static_cast<ForDebuggingOnlyBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  std::string url_string;
  if (args.Length() < 1 || args[0].IsEmpty() ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[0], &url_string)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "reportAdAuctionWin requires 1 string parameter")));
    return;
  }

  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "reportAdAuctionWin must be passed a valid HTTPS url")));
    return;
  }
  bindings->win_report_url_ = url;
}

}  // namespace auction_worklet
