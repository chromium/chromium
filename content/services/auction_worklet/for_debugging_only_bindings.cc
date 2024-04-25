// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/for_debugging_only_bindings.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

// Attempts to parse the first elements of `args` as a URL, and set `url_out` to
// it. Throws an exception on most failures and leaves `url_out` alone.
// `function_name` is used only for exception text and logging.
void ParseAndSetDebugUrl(AuctionV8Helper* v8_helper,
                         AuctionV8Logger* v8_logger,
                         const v8::FunctionCallbackInfo<v8::Value>& args,
                         const char* function_name,
                         std::optional<GURL>& url_out) {
  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               base::StrCat({function_name, "(): "}), &args,
                               /*min_required_args=*/1);

  std::string url_string;
  if (!args_converter.ConvertArg(0, "url", url_string)) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    args.GetIsolate()->ThrowException(
        // This is not actually a string literal, but it consists of two
        // concatenated string literals. The important bit is that they're fixed
        // strings, so this shouldn't fail.
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            base::StrCat({function_name, " must be passed a valid HTTPS url"})
                .c_str())));
    return;
  }

  // If the URL is too long, log an error to console and clear the URL, rather
  // than throwing. URL limits aren't specified, and soft-failing is more
  // consistent with other behavior around this case.
  if (url.spec().size() > url::kMaxURLChars) {
    url_out = std::nullopt;
    // Don't print out full URL in this case, since it will fill the entire
    // console.
    v8_logger->LogConsoleWarning(
        base::StrCat({function_name, " accepts URLs of at most length ",
                      base::NumberToString(url::kMaxURLChars), "."}));
    return;
  }

  url_out = std::move(url);
}

}  // namespace

ForDebuggingOnlyBindings::ForDebuggingOnlyBindings(AuctionV8Helper* v8_helper,
                                                   AuctionV8Logger* v8_logger)
    : v8_helper_(v8_helper), v8_logger_(v8_logger) {}

ForDebuggingOnlyBindings::~ForDebuggingOnlyBindings() = default;

void ForDebuggingOnlyBindings::AttachToContext(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::External> v8_this = v8::External::New(isolate, this);
  v8::Local<v8::Object> debugging = v8::Object::New(isolate);

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
  debugging
      ->Set(context, v8_helper_->CreateStringFromLiteral("reportAdAuctionLoss"),
            loss_template->GetFunction(context).ToLocalChecked())
      .Check();

  win_template->RemovePrototype();
  debugging
      ->Set(context, v8_helper_->CreateStringFromLiteral("reportAdAuctionWin"),
            win_template->GetFunction(context).ToLocalChecked())
      .Check();

  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("forDebuggingOnly"),
            debugging)
      .Check();
}

void ForDebuggingOnlyBindings::Reset() {
  loss_report_url_ = std::nullopt;
  win_report_url_ = std::nullopt;
}

std::optional<GURL> ForDebuggingOnlyBindings::TakeLossReportUrl() {
  // `loss_report_url_` should never be an invalid URL.
  DCHECK(!loss_report_url_ || loss_report_url_->is_valid());
  return std::move(loss_report_url_);
}

std::optional<GURL> ForDebuggingOnlyBindings::TakeWinReportUrl() {
  // `win_report_url_` should never be an invalid URL.
  DCHECK(!win_report_url_ || win_report_url_->is_valid());
  return std::move(win_report_url_);
}

void ForDebuggingOnlyBindings::ReportAdAuctionLoss(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ForDebuggingOnlyBindings* bindings = static_cast<ForDebuggingOnlyBindings*>(
      v8::External::Cast(*args.Data())->Value());
  ParseAndSetDebugUrl(bindings->v8_helper_.get(), bindings->v8_logger_.get(),
                      args, "reportAdAuctionLoss", bindings->loss_report_url_);
}

void ForDebuggingOnlyBindings::ReportAdAuctionWin(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ForDebuggingOnlyBindings* bindings = static_cast<ForDebuggingOnlyBindings*>(
      v8::External::Cast(*args.Data())->Value());
  ParseAndSetDebugUrl(bindings->v8_helper_.get(), bindings->v8_logger_.get(),
                      args, "reportAdAuctionWin", bindings->win_report_url_);
}

}  // namespace auction_worklet
