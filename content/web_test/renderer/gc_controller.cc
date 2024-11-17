// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/gc_controller.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace content {

gin::WrapperInfo GCController::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void GCController::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GCController> controller =
      gin::CreateHandle(isolate, new GCController(frame));
  if (controller.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "GCController"),
            controller.ToV8())
      .Check();
}

GCController::GCController(blink::WebLocalFrame* frame) : frame_(frame) {}

GCController::~GCController() = default;

gin::ObjectTemplateBuilder GCController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GCController>::GetObjectTemplateBuilder(isolate)
      .SetMethod("collect", &GCController::Collect)
      .SetMethod("collectAll", &GCController::CollectAll)
      .SetMethod("minorCollect", &GCController::MinorCollect)
      .SetMethod("asyncCollectAll", &GCController::AsyncCollectAll);
}

void GCController::Collect(const gin::Arguments& args) {
  args.isolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

void GCController::CollectAll(const gin::Arguments& args) {
  for (int i = 0; i < kNumberOfGCsForFullCollection; i++) {
    args.isolate()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }
}

void GCController::AsyncCollectAll(const gin::Arguments& args) {
  v8::HandleScope scope(args.isolate());

  if (args.PeekNext().IsEmpty() || !args.PeekNext()->IsFunction()) {
    args.ThrowTypeError(
        "asyncCollectAll should be called with a callback argument being a "
        "v8::Function.");
    return;
  }
  v8::UniquePersistent<v8::Function> js_callback(
      args.isolate(), v8::Local<v8::Function>::Cast(args.PeekNext()));

  // Bind the js callback into a OnceClosure that will be run asynchronously in
  // a fresh call stack.
  base::OnceClosure run_async =
      base::BindOnce(&GCController::AsyncCollectAllWithEmptyStack,
                     weak_ptr_factory_.GetWeakPtr(), std::move(js_callback));
  frame_->GetTaskRunner(blink::TaskType::kInternalTest)
      ->PostTask(FROM_HERE, std::move(run_async));
}

void GCController::AsyncCollectAllWithEmptyStack(
    v8::UniquePersistent<v8::Function> callback) {
  v8::Isolate* const isolate = frame_->GetAgentGroupScheduler()->Isolate();

  for (int i = 0; i < kNumberOfGCsForFullCollection; i++) {
    isolate->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection,
        cppgc::EmbedderStackState::kNoHeapPointers);
  }

  v8::HandleScope scope(isolate);
  v8::Local<v8::Function> func = callback.Get(isolate);
  v8::Local<v8::Context> context = func->GetCreationContextChecked(isolate);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto result = func->Call(context, context->Global(), 0, nullptr);
  // Swallow potential exception.
  std::ignore = result;
}

void GCController::MinorCollect(const gin::Arguments& args) {
  args.isolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kMinorGarbageCollection);
}

}  // namespace content
