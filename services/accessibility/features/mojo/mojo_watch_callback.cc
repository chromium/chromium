// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/mojo/mojo_watch_callback.h"

#include <memory>

#include "gin/public/context_holder.h"
#include "mojo/public/c/system/types.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"

namespace ax {

MojoWatchCallback::MojoWatchCallback(
    std::unique_ptr<gin::ContextHolder> context_holder,
    v8::Local<v8::Function> callback)
    : context_holder_(std::move(context_holder)),
      callback_(context_holder_->isolate(), callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MojoWatchCallback::~MojoWatchCallback() = default;

void MojoWatchCallback::Call(MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Enter the isolate, handle, and context scopes in order to call the
  // callback.
  v8::Isolate* isolate = context_holder_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_holder_->context();
  v8::Context::Scope scope(context);

  v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, result)};
  callback_.Get(isolate)
      ->Call(context, context->Global(), 1, args)
      .ToLocalChecked();
}

}  // namespace ax
