// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gc_callback.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       const v8::Local<v8::Function>& callback,
                       const base::Closure& fallback)
    : GCCallback(context, object, callback, base::Closure(), fallback) {}

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       const base::Closure& callback,
                       const base::Closure& fallback)
    : GCCallback(context,
                 object,
                 v8::Local<v8::Function>(),
                 callback,
                 fallback) {}

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       const v8::Local<v8::Function> v8_callback,
                       const base::Closure& closure_callback,
                       const base::Closure& fallback)
    : context_(context),
      object_(context->isolate(), object),
      closure_callback_(closure_callback),
      fallback_(fallback) {
  DCHECK(closure_callback_ || !v8_callback.IsEmpty());
  if (!v8_callback.IsEmpty())
    v8_callback_.Reset(context->isolate(), v8_callback);
  object_.SetWeak(this, OnObjectGC, v8::WeakCallbackType::kParameter);
  context->AddInvalidationObserver(base::BindOnce(
      &GCCallback::OnContextInvalidated, weak_ptr_factory_.GetWeakPtr()));
}

GCCallback::~GCCallback() {}

// static
void GCCallback::OnObjectGC(const v8::WeakCallbackInfo<GCCallback>& data) {
  // Usually FirstWeakCallback should do nothing other than reset |object_|
  // and then set a second weak callback to run later. We can sidestep that,
  // because posting a task to the current message loop is all but free - but
  // DO NOT add any more work to this method. The only acceptable place to add
  // code is RunCallback.
  GCCallback* self = data.GetParameter();
  self->object_.Reset();

  blink::WebLocalFrame* frame = self->context_->web_frame();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  if (frame) {
    task_runner = frame->GetTaskRunner(blink::TaskType::kInternalDefault);
  } else {
    // |frame| can be null on tests.
    task_runner = base::ThreadTaskRunnerHandle::Get();
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&GCCallback::RunCallback,
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
    closure_callback_.Run();
  }
  delete this;
}

void GCCallback::OnContextInvalidated() {
  if (!fallback_.is_null())
    fallback_.Run();
  delete this;
}

}  // namespace extensions
