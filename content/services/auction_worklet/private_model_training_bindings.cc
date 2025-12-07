// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/private_model_training_bindings.h"

#include "base/functional/callback.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "gin/public/gin_embedders.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function.h"

namespace auction_worklet {

PrivateModelTrainingBindings::PrivateModelTrainingBindings(
    AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

PrivateModelTrainingBindings::~PrivateModelTrainingBindings() = default;

void PrivateModelTrainingBindings::AttachToContext(
    v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this = v8::External::New(
      v8_helper_->isolate(), this, gin::kPrivateModelTrainingBindingsTag);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(context, &PrivateModelTrainingBindings::SendEncryptedTo,
                        v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("sendEncryptedTo"),
            v8_function)
      .Check();
}

void PrivateModelTrainingBindings::Reset() {
  payload_ = std::nullopt;
  already_called_ = false;
}

void PrivateModelTrainingBindings::SendEncryptedTo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateModelTrainingBindings* bindings =
      static_cast<PrivateModelTrainingBindings*>(
          v8::External::Cast(*args.Data())
              ->Value(gin::kPrivateModelTrainingBindingsTag));
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "sendEncryptedTo(): ", &args,
                               /*min_required_args=*/1);
  v8::Local<v8::Value> unencrypted_payload;
  if (!args_converter.ConvertArg(0, "payload", unencrypted_payload)) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  // Type check
  if (!unencrypted_payload->IsArrayBuffer()) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendEncryptedTo() may only take a ArrayBuffer")));
    return;
  }

  v8::Local<v8::ArrayBuffer> array_buffer =
      unencrypted_payload.As<v8::ArrayBuffer>();

  if (array_buffer->IsResizableByUserJavaScript()) {
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendEncryptedTo() may not be resizable by user JavaScript")));
    return;
  }

  // Semantic Checks
  if (bindings->already_called_) {
    bindings->payload_.reset();
    args.GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "sendEncryptedTo() may be called at most once")));
    return;
  }

  // Convert the array buffer to a mojom_base::BigBuffer
  size_t buffer_size = array_buffer->ByteLength();
  void* buffer_data = array_buffer->Data();

  // SAFETY: The V8 ArrayBuffer handles the data's lifetime and `buffer_size`
  // comes from ByteLength(). BigBuffer copies the data, and we only use the
  // span for this copy. The browser process will copy the data again before
  // using it to prevent TOCTOU issues.
  base::span<uint8_t> data_span = UNSAFE_BUFFERS(
      base::span<uint8_t>(static_cast<uint8_t*>(buffer_data), buffer_size));

  mojo_base::BigBuffer big_buffer(data_span);

  bindings->payload_ = std::move(big_buffer);
  bindings->already_called_ = true;
}

}  // namespace auction_worklet
