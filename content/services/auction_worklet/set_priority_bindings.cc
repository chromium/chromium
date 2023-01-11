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
#include "gin/converter.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

SetPriorityBindings::SetPriorityBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

SetPriorityBindings::~SetPriorityBindings() = default;

void SetPriorityBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::FunctionTemplate> v8_template = v8::FunctionTemplate::New(
      v8_helper_->isolate(), &SetPriorityBindings::SetPriority, v8_this);
  v8_template->RemovePrototype();
  global_template->Set(v8_helper_->CreateStringFromLiteral("setPriority"),
                       v8_template);
}

void SetPriorityBindings::Reset() {
  set_priority_ = absl::nullopt;
  exception_thrown_ = false;
}

void SetPriorityBindings::SetPriority(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  SetPriorityBindings* bindings = static_cast<SetPriorityBindings*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  double set_priority;
  if (args.Length() < 1 || args[0].IsEmpty() ||
      !gin::ConvertFromV8(v8_helper->isolate(), args[0], &set_priority)) {
    bindings->exception_thrown_ = true;
    bindings->set_priority_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "setPriority requires 1 double parameter")));
    return;
  }

  if (!std::isfinite(set_priority)) {
    bindings->exception_thrown_ = true;
    bindings->set_priority_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "setPriority requires 1 finite double parameter")));
    return;
  }

  if (bindings->exception_thrown_ || bindings->set_priority_) {
    bindings->exception_thrown_ = true;
    bindings->set_priority_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "setPriority may be called at most once")));
    return;
  }

  bindings->set_priority_ = set_priority;
}

}  // namespace auction_worklet
