// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/set_priority_bindings.h"

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

SetPriorityBindings::SetPriorityBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

SetPriorityBindings::~SetPriorityBindings() = default;

void SetPriorityBindings::AttachToContext(v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(context, &SetPriorityBindings::SetPriority, v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("setPriority"),
            v8_function)
      .Check();
}

void SetPriorityBindings::Reset() {
  set_priority_ = std::nullopt;
  already_called_ = false;
}

void SetPriorityBindings::SetPriority(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SetPriorityBindings* bindings = static_cast<SetPriorityBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "setPriority(): ", &args,
                               /*min_required_args=*/1);

  double set_priority;
  if (!args_converter.ConvertArg(0, "priority", set_priority)) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    // Note that we do not set `already_called_` here since in spec-land the
    // call did not actually happen.
    return;
  }

  if (bindings->already_called_) {
    bindings->set_priority_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "setPriority may be called at most once")));
    return;
  }
  bindings->already_called_ = true;
  bindings->set_priority_ = set_priority;
}

}  // namespace auction_worklet
