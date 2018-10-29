// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/gc_controller.h"

#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace test_runner {

gin::WrapperInfo GCController::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void GCController::Install(TestInterfaces* interfaces,
                           blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GCController> controller =
      gin::CreateHandle(isolate, new GCController(interfaces));
  if (controller.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "GCController"), controller.ToV8());
}

GCController::GCController(TestInterfaces* interfaces)
    : interfaces_(interfaces) {}

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

  v8::UniquePersistent<v8::Function> func(
      args.isolate(), v8::Local<v8::Function>::Cast(args.PeekNext()));

  CHECK(interfaces_->GetDelegate());
  CHECK(!func.IsEmpty());
  interfaces_->GetDelegate()->PostTask(
      base::BindOnce(&GCController::AsyncCollectAllWithEmptyStack,
                     base::Unretained(this), std::move(func)));
}

void GCController::AsyncCollectAllWithEmptyStack(
    v8::UniquePersistent<v8::Function> callback) {
  v8::Isolate* const isolate = blink::MainThreadIsolate();

  for (int i = 0; i < kNumberOfGCsForFullCollection; i++) {
    isolate->GetEmbedderHeapTracer()->GarbageCollectionForTesting(
        v8::EmbedderHeapTracer::kEmpty);
  }

  v8::HandleScope scope(isolate);
  v8::Local<v8::Function> func = callback.Get(isolate);
  v8::Local<v8::Context> context = func->CreationContext();
  v8::Context::Scope context_scope(context);
  func->Call(context, v8::Undefined(isolate), 0, nullptr).ToLocalChecked();
}

void GCController::MinorCollect(const gin::Arguments& args) {
  args.isolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kMinorGarbageCollection);
}

}  // namespace test_runner
