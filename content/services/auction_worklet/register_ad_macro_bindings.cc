// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/register_ad_macro_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "gin/converter.h"
#include "third_party/blink/public/common/features.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"

namespace auction_worklet {

RegisterAdMacroBindings::RegisterAdMacroBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

RegisterAdMacroBindings::~RegisterAdMacroBindings() = default;

void RegisterAdMacroBindings::AttachToContext(v8::Local<v8::Context> context) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdAuctionReportingWithMacroApi)) {
    return;
  }

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(context, &RegisterAdMacroBindings::RegisterAdMacro,
                        v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("registerAdMacro"),
            v8_function)
      .Check();
}

void RegisterAdMacroBindings::Reset() {
  ad_macro_map_.clear();
}

void RegisterAdMacroBindings::RegisterAdMacro(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  RegisterAdMacroBindings* bindings = static_cast<RegisterAdMacroBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  std::string macro_name;
  std::string macro_value;
  // Any additional arguments are ignored.
  if (args.Length() < 2 || args[0].IsEmpty() || args[1].IsEmpty() ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[0], &macro_name) ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[1], &macro_value)) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "registerAdMacro requires 2 string parameters")));
    return;
  }

  bindings->ad_macro_map_[macro_name] = macro_value;
}

}  // namespace auction_worklet
