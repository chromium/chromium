// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gc_callback.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       const v8::Local<v8::Function>& callback,
                       base::OnceClosure fallback)
    : GCCallback(context,
                 object,
                 callback,
                 base::OnceClosure(),
                 std::move(fallback)) {}

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       base::OnceClosure callback,
                       base::OnceClosure fallback)
    : GCCallback(context,
                 object,
                 v8::Local<v8::Function>(),
                 std::move(callback),
                 std::move(fallback)) {}

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       const v8::Local<v8::Function> v8_callback,
                       base::OnceClosure closure_callback,
                       base::OnceClosure fallback)
    : context_(context),
      object_(context->isolate(), object),
      closure_callback_(std::move(closure_callback)),
      fallback_(std::move(fallback)) {
  DCHECK(closure_callback_ || !v8_callback.IsEmpty());
  if (!v8_callback.IsEmpty())
    v8_callback_.Reset(context->isolate(), v8_callback);
  object_.SetWeak(this, OnObjectGC, v8::WeakCallbackType::kParameter);
  context->AddInvalidationObserver(base::BindOnce(
      &GCCallback::OnContextInvalidated, weak_ptr_factory_.GetWeakPtr()));
  blink::WebLocalFrame* frame = context_->web_frame();
  if (frame) {
    // We cache the task runner here instead of fetching it right before we use
    // it to avoid a potential crash that can happen if the object is GC'd
    // *during* the script context invalidation process (e.g. before the
    // extension system has marked the context invalid but after the frame has
    // already had it's schedueler disconnected). See crbug.com/1216541
    task_runner_ = frame->GetTaskRunner(blink::TaskType::kInternalDefault);
  } else {
    // |frame| can be null on tests.
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }
}

GCCallback::~GCCallback() = default;

// static
void GCCallback::OnObjectGC(const v8::WeakCallbackInfo<GCCallback>& data) {
  // Usually FirstWeakCallback should do nothing other than reset |object_|
  // and then set a second weak callback to run later. We can sidestep that,
  // because posting a task to the current message loop is all but free - but
  // DO NOT add any more work to this method. The only acceptable place to add
  // code is RunCallback.
  GCCallback* self = data.GetParameter();
  self->object_.Reset();

  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCCallback::RunCallback,
                                self->weak_ptr_factory_.GetWeakPtr()));
}

void GCCallback::RunCallback() {
  fallback_.Reset();
  if (!v8_callback_.IsEmpty()) {
    v8::Isolate* isolate = context_->isolate();
    v8::HandleScope handle_scope(isolate);
    context_->SafeCallFunction(
        v8::Local<v8::Function>::New(isolate, v8_callback_), 0, nullptr);
  } else if (closure_callback_) {
    std::move(closure_callback_).Run();
  }
  delete this;
}

void GCCallback::OnContextInvalidated() {
  if (!fallback_.is_null())
    std::move(fallback_).Run();
  delete this;
}

}  // namespace extensions
