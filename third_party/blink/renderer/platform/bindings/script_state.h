// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_STATE_H_

#include <memory>

#include "gin/public/context_holder.h"
#include "gin/public/gin_embedders.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/v8_cross_origin_callback_info.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class ScriptValue;
class V8PerContextData;

// ScriptState is an abstraction class that holds all information about script
// exectuion (e.g., v8::Isolate, v8::Context, DOMWrapperWorld, ExecutionContext
// etc). If you need any info about the script execution, you're expected to
// pass around ScriptState in the code base. ScriptState is in a 1:1
// relationship with v8::Context.
//
// When you need ScriptState, you can add [CallWith=ScriptState] to IDL files
// and pass around ScriptState into a place where you need ScriptState.
//
// In some cases, you need ScriptState in code that doesn't have any JavaScript
// on the stack. Then you can store ScriptState on a C++ object using
// Member<ScriptState> or Persistent<ScriptState>.
//
// class SomeObject : public GarbageCollected<SomeObject> {
//   void someMethod(ScriptState* scriptState) {
//     script_state_ = scriptState; // Record the ScriptState.
//     ...;
//   }
//
//   void asynchronousMethod() {
//     if (!script_state_->contextIsValid()) {
//       // It's possible that the context is already gone.
//       return;
//     }
//     // Enter the ScriptState.
//     ScriptState::Scope scope(script_state_);
//     // Do V8 related things.
//     ToV8(...);
//   }
//
//   virtual void Trace(Visitor* visitor) {
//     visitor->Trace(script_state_);  // ScriptState also needs to be traced.
//   }
//
//   Member<ScriptState> script_state_;
// };
//
// You should not store ScriptState on a C++ object that can be accessed
// by multiple worlds. For example, you can store ScriptState on
// ScriptPromiseResolver, ScriptValue etc because they can be accessed from one
// world. However, you cannot store ScriptState on a DOM object that has
// an IDL interface because the DOM object can be accessed from multiple
// worlds. If ScriptState of one world "leak"s to another world, you will
// end up with leaking any JavaScript objects from one Chrome extension
// to another Chrome extension, which is a severe security bug.
//
// Lifetime:
// ScriptState is created when v8::Context is created.
// ScriptState is destroyed when v8::Context is garbage-collected and
// all V8 proxy objects that have references to the ScriptState are destructed.
class PLATFORM_EXPORT ScriptState final : public GarbageCollected<ScriptState> {
 public:
  class Scope {
    STACK_ALLOCATED();

   public:
    // You need to make sure that scriptState->context() is not empty before
    // creating a Scope.
    explicit Scope(ScriptState* script_state)
        : handle_scope_(script_state->GetIsolate()),
          context_(script_state->GetContext()) {
      DCHECK(script_state->ContextIsValid());
      context_->Enter();
    }

    ~Scope() { context_->Exit(); }

   private:
    v8::HandleScope handle_scope_;
    v8::Local<v8::Context> context_;
  };

  ScriptState(v8::Local<v8::Context>, scoped_refptr<DOMWrapperWorld>);
  ~ScriptState();

  void Trace(blink::Visitor*) {}

  static ScriptState* Current(v8::Isolate* isolate) {  // DEPRECATED
    return From(isolate->GetCurrentContext());
  }

  static ScriptState* ForCurrentRealm(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    return From(info.GetIsolate()->GetCurrentContext());
  }

  static ScriptState* ForCurrentRealm(
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    return From(info.GetIsolate()->GetCurrentContext());
  }

  static ScriptState* ForRelevantRealm(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    return From(info.Holder()->CreationContext());
  }

  static ScriptState* ForRelevantRealm(const V8CrossOriginCallbackInfo& info) {
    return From(info.Holder()->CreationContext());
  }

  static ScriptState* ForRelevantRealm(
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    return From(info.Holder()->CreationContext());
  }

  static ScriptState* ForRelevantRealm(
      const v8::PropertyCallbackInfo<void>& info) {
    return From(info.Holder()->CreationContext());
  }

  static ScriptState* From(v8::Local<v8::Context> context) {
    DCHECK(!context.IsEmpty());
    ScriptState* script_state =
        static_cast<ScriptState*>(context->GetAlignedPointerFromEmbedderData(
            kV8ContextPerContextDataIndex));
    // ScriptState::from() must not be called for a context that does not have
    // valid embedder data in the embedder field.
    SECURITY_CHECK(script_state);
    SECURITY_CHECK(script_state->context_ == context);
    return script_state;
  }

  v8::Isolate* GetIsolate() const { return isolate_; }
  DOMWrapperWorld& World() const { return *world_; }

  // This can return an empty handle if the v8::Context is gone.
  v8::Local<v8::Context> GetContext() const {
    return context_.NewLocal(isolate_);
  }
  bool ContextIsValid() const {
    return !context_.IsEmpty() && per_context_data_;
  }
  void DetachGlobalObject();

  V8PerContextData* PerContextData() const { return per_context_data_.get(); }
  void DisposePerContextData();

  // This method is expected to be called only from
  // WorkerOrWorkletScriptController to run operations that should have been
  // invoked by a weak callback if a V8 GC were run, in a worker thread
  // termination.
  void DissociateContext();

 private:
  static void OnV8ContextCollectedCallback(
      const v8::WeakCallbackInfo<ScriptState>&);

  v8::Isolate* isolate_;
  // This persistent handle is weak.
  ScopedPersistent<v8::Context> context_;

  // This refptr doesn't cause a cycle because all persistent handles that
  // DOMWrapperWorld holds are weak.
  scoped_refptr<DOMWrapperWorld> world_;

  // This std::unique_ptr causes a cycle:
  // V8PerContextData --(Persistent)--> v8::Context --(RefPtr)--> ScriptState
  //     --(std::unique_ptr)--> V8PerContextData
  // So you must explicitly clear the std::unique_ptr by calling
  // disposePerContextData() once you no longer need V8PerContextData.
  // Otherwise, the v8::Context will leak.
  std::unique_ptr<V8PerContextData> per_context_data_;

  // v8::Context has an internal field to this ScriptState* as a raw pointer,
  // which is out of scope of Blink GC, but it must be a strong reference.  We
  // use |reference_from_v8_context_| to represent this strong reference.  The
  // lifetime of |reference_from_v8_context_| and the internal field must match
  // exactly.
  SelfKeepAlive<ScriptState> reference_from_v8_context_;

  static constexpr int kV8ContextPerContextDataIndex = static_cast<int>(
      gin::kPerContextDataStartIndex +  // NOLINT(readability/enum_casing)
      gin::kEmbedderBlink);             // NOLINT(readability/enum_casing)

  DISALLOW_COPY_AND_ASSIGN(ScriptState);
};

// ScriptStateProtectingContext keeps the context associated with the
// ScriptState alive.  You need to call Clear() once you no longer need the
// context. Otherwise, the context will leak.
class ScriptStateProtectingContext final
    : public GarbageCollected<ScriptStateProtectingContext> {
 public:
  explicit ScriptStateProtectingContext(ScriptState* script_state)
      : script_state_(script_state) {
    if (script_state_) {
      context_.Set(script_state_->GetIsolate(), script_state_->GetContext());
      context_.Get().AnnotateStrongRetainer(
          "blink::ScriptStateProtectingContext::context_");
    }
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(script_state_); }

  ScriptState* Get() const { return script_state_; }
  void Reset() {
    script_state_ = nullptr;
    context_.Clear();
  }

  // ScriptState like interface
  bool ContextIsValid() const { return script_state_->ContextIsValid(); }
  v8::Isolate* GetIsolate() const { return script_state_->GetIsolate(); }
  v8::Local<v8::Context> GetContext() const {
    return script_state_->GetContext();
  }

 private:
  Member<ScriptState> script_state_;
  ScopedPersistent<v8::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(ScriptStateProtectingContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_STATE_H_
