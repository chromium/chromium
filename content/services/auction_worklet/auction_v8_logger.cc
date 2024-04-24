// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_logger.h"

#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-value.h"

namespace auction_worklet {

AuctionV8Logger::AuctionV8Logger(AuctionV8Helper* v8_helper,
                                 v8::Local<v8::Context> context)
    : v8_helper_(v8_helper) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Object> console = v8::Local<v8::Object>::Cast(
      context->Global()
          ->Get(context, v8_helper_->CreateStringFromLiteral("console"))
          .ToLocalChecked());
  v8::Local<v8::Function> warn = v8::Local<v8::Function>::Cast(
      console->Get(context, v8_helper_->CreateStringFromLiteral("warn"))
          .ToLocalChecked());

  console_warn_.Reset(isolate, warn);
}

AuctionV8Logger::~AuctionV8Logger() = default;

void AuctionV8Logger::LogConsoleWarning(std::string_view message) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Function> console_warn = console_warn_.Get(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::String> v8_string;
  if (!v8_helper_->CreateUtf8String(message).ToLocal(&v8_string)) {
    // Drop message on error.
    return;
  }

  v8::LocalVector<v8::Value> args(isolate);
  args.emplace_back(std::move(v8_string));

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper_->GetTimeLimit());
  v8::TryCatch try_catch(isolate);
  v8::MaybeLocal<v8::Value> result =
      console_warn->Call(context, context->Global(), args.size(), args.data());
  if (result.IsEmpty()) {
    // console.warn() should not throw, so the only error that can happen should
    // be a timeout.
    DCHECK(try_catch.HasTerminated());
  }
}

}  // namespace auction_worklet
