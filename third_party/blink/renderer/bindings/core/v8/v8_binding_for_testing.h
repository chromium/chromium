// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_BINDING_FOR_TESTING_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_BINDING_FOR_TESTING_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class DummyPageHolder;
class ExecutionContext;
class LocalFrame;
class KURL;
class Page;

class V8TestingScope {
  STACK_ALLOCATED();

 public:
  // TODO(keishi): Define CreateDummyPageHolder in DummyPageHolder.
  static std::unique_ptr<DummyPageHolder> CreateDummyPageHolder(
      const KURL& url);
  explicit V8TestingScope(const KURL& url = KURL());
  ScriptState* GetScriptState() const;
  ExecutionContext* GetExecutionContext() const;
  v8::Isolate* GetIsolate() const;
  v8::Local<v8::Context> GetContext() const;
  ExceptionState& GetExceptionState();
  Page& GetPage();
  LocalFrame& GetFrame();
  Document& GetDocument();
  ~V8TestingScope();

 private:
  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
  v8::TryCatch try_catch_;
  DummyExceptionStateForTesting exception_state_;
};

// Similar to other ToV8 helpers in to_v8_for_core.h.
template <typename T>
v8::Local<v8::Value> ToV8(V8TestingScope* scope, T value) {
  return blink::ToV8(value, scope->GetContext()->Global(), scope->GetIsolate());
}

// Test supporting different kinds of GCs.
class BindingTestSupportingGC : public testing::Test {
 public:
  void SetIsolate(v8::Isolate* isolate) {
    CHECK(isolate);
    CHECK_EQ(isolate, ThreadState::Current()->GetIsolate());
    isolate_ = isolate;
  }
  v8::Isolate* GetIsolate() const { return isolate_; }

  void PreciselyCollectGarbage() {
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  void RunV8MinorGC() {
    isolate_->RequestGarbageCollectionForTesting(
        v8::Isolate::GarbageCollectionType::kMinorGarbageCollection);
  }

  void RunV8FullGC(v8::EmbedderHeapTracer::EmbedderStackState stack_state =
                       v8::EmbedderHeapTracer::EmbedderStackState::kEmpty) {
    V8GCController::CollectAllGarbageForTesting(isolate_, stack_state);
  }

 private:
  v8::Isolate* isolate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_BINDING_FOR_TESTING_H_
