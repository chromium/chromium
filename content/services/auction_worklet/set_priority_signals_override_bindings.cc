// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/set_priority_signals_override_bindings.h"

#include <cmath>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"

namespace auction_worklet {

SetPrioritySignalsOverrideBindings::SetPrioritySignalsOverrideBindings(
    AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

SetPrioritySignalsOverrideBindings::~SetPrioritySignalsOverrideBindings() =
    default;

void SetPrioritySignalsOverrideBindings::AttachToContext(
    v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(
          context,
          &SetPrioritySignalsOverrideBindings::SetPrioritySignalsOverride,
          v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context,
            v8_helper_->CreateStringFromLiteral("setPrioritySignalsOverride"),
            v8_function)
      .Check();
}

void SetPrioritySignalsOverrideBindings::Reset() {
  update_priority_signals_overrides_.clear();
}

base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
SetPrioritySignalsOverrideBindings::TakeSetPrioritySignalsOverrides() {
  return std::move(update_priority_signals_overrides_);
}

// static
void SetPrioritySignalsOverrideBindings::SetPrioritySignalsOverride(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SetPrioritySignalsOverrideBindings* bindings =
      static_cast<SetPrioritySignalsOverrideBindings*>(
          v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "setPrioritySignalsOverride(): ", &args,
                               /*min_required_args=*/1);

  std::string key;
  args_converter.ConvertArg(0, "key", key);

  mojom::PrioritySignalsDoublePtr mojom_value;
  // In case of only one argument, or the second argument is null or undefined,
  // use nullopt. Otherwise, the second argument must be a number.
  if (args_converter.is_success() && args.Length() >= 2 &&
      !args[1]->IsNullOrUndefined()) {
    double double_value;
    if (args_converter.ConvertArg(1, "priority", double_value)) {
      mojom_value = mojom::PrioritySignalsDouble::New(double_value);
    }
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
  } else {
    bindings->update_priority_signals_overrides_.insert_or_assign(
        std::move(key), std::move(mojom_value));
  }
}

}  // namespace auction_worklet
