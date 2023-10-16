// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_FUNCTION_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_FUNCTION_BASE_H_

#include "base/functional/callback.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"

namespace blink {

// CallbackFunctionBase is the common base class of all the callback function
// classes. Most importantly this class provides a way of type dispatching (e.g.
// overload resolutions, SFINAE technique, etc.) so that it's possible to
// distinguish callback functions from anything else. Also it provides a common
// implementation of callback functions.
// This base class does not provide support for task attribution, so the
// callback that require such a functionality should inherit from
// CallbackFunctionWithTaskAttributionBase class.
//
// As the signatures of callback functions vary, this class does not implement
// |Invoke| member function that performs "invoke" steps. Subclasses will
// implement it.
class PLATFORM_EXPORT CallbackFunctionBase
    : public GarbageCollected<CallbackFunctionBase>,
      public NameClient {
 public:
  ~CallbackFunctionBase() override = default;

  virtual void Trace(Visitor* visitor) const;

  v8::Local<v8::Object> CallbackObject() {
    return callback_function_.Get(GetIsolate());
  }

  v8::Isolate* GetIsolate() const {
    if (!cached_data_) {
      // It's generally faster to get Isolate from the ScriptState object but
      // as long as we don't have one load the Isolate from the callback.
      return v8::Object::GetIsolate(callback_function_);
    }
    return cached_data_->incumbent_script_state_->GetIsolate();
  }

  // Returns the ScriptState of the relevant realm of the callback object.
  //
  // NOTE: This function must be used only when it's pretty sure that the
  // callback object is the same origin-domain. Otherwise,
  // |CallbackRelevantScriptStateOrReportError| or
  // |CallbackRelevantScriptStateOrThrowException| must be used instead.
  ScriptState* CallbackRelevantScriptState() const {
    ScriptState* script_state = CallbackRelevantScriptStateImpl();
    DCHECK(script_state);
    return script_state;
  }

  // Returns the ScriptState of the relevant realm of the callback object iff
  // the callback is the same origin-domain. Otherwise, reports an error and
  // returns nullptr.
  ScriptState* CallbackRelevantScriptStateOrReportError(
      const char* interface_name,
      const char* operation_name);

  // Returns the ScriptState of the relevant realm of the callback object iff
  // the callback is the same origin-domain. Otherwise, throws an exception and
  // returns nullptr.
  ScriptState* CallbackRelevantScriptStateOrThrowException(
      const char* interface_name,
      const char* operation_name);

  ScriptState* IncumbentScriptState() const {
    if (!cached_data_) {
      MakeCachedData();
    }
    return cached_data_->incumbent_script_state_.Get();
  }

  DOMWrapperWorld& GetWorld() const { return IncumbentScriptState()->World(); }

  // Returns true if the ES function has a [[Construct]] internal method.
  bool IsConstructor() const { return CallbackFunction()->IsConstructor(); }

  // Evaluates the given |closure| as part of the IDL callback function, i.e.
  // in the relevant realm of the callback object with the callback context as
  // the incumbent realm.
  //
  // NOTE: Do not abuse this function.  Let |Invoke| method defined in a
  // subclass do the right thing.  This function is rarely needed.
  void EvaluateAsPartOfCallback(base::OnceCallback<void(ScriptState*)> closure);

  // Makes the underlying V8 function collectable by V8 Scavenger GC.  Do not
  // use this function unless you really need a hacky performance optimization.
  // The V8 function is collectable by V8 Full GC whenever this instance is no
  // longer referenced, so there is no need to call this function unless you
  // really need V8 *Scavenger* GC to collect the V8 function before V8 Full GC
  // runs.
  void DisposeV8FunctionImmediatelyToReduceMemoryFootprint() {
    callback_function_.Reset();
  }

 protected:
  explicit CallbackFunctionBase(v8::Local<v8::Object>);

  v8::Local<v8::Function> CallbackFunction() const {
    return callback_function_.Get(GetIsolate()).As<v8::Function>();
  }

 private:
  ScriptState* CallbackRelevantScriptStateImpl() const {
    if (!cached_data_) {
      MakeCachedData();
    }
    return cached_data_->callback_relevant_script_state_.Get();
  }

  inline void MakeCachedData(ScriptState* callback_relevant_script_state,
                             ScriptState* incumbent_script_state) const;

  // Computes callback relevant script state and stores it in the cached data.
  // If the cached data wasn't created in constructor then the incumbent
  // script state is the same as the relevant script state.
  void MakeCachedData() const;

  // This object is a container for rarely necessary fields which are needed
  // only when
  //  - the callback is actually called,
  //  - the incumbent script state is different from the relevant script state,
  //  - task attribution tracking is enabled.
  class CachedData final : public GarbageCollected<CachedData> {
   public:
    CachedData(ScriptState* callback_relevant_script_state,
               ScriptState* incumbent_script_state)
        : callback_relevant_script_state_(callback_relevant_script_state),
          incumbent_script_state_(incumbent_script_state) {}

    void Trace(Visitor* visitor) const;

    // The associated Realm of the callback function type value iff it's the
    // same origin-domain. Otherwise, nullptr.
    Member<ScriptState> callback_relevant_script_state_;
    // The callback context, i.e. the incumbent Realm when an ECMAScript value
    // is converted to an IDL value.
    // https://webidl.spec.whatwg.org/#dfn-callback-context
    Member<ScriptState> incumbent_script_state_;
  };

  // The "callback function type" value.
  // Use v8::Object instead of v8::Function in order to handle
  // [LegacyTreatNonObjectAsNull].
  // TODO(1420942): consider storing either v8::Function or v8::Context in this
  // field in order to simplify lazy creation of CachedData object.
  TraceWrapperV8Reference<v8::Object> callback_function_;
  // Pointer to lazily computed data which is not needed in most of the cases.
  mutable Member<CachedData> cached_data_;
};

// CallbackFunctionWithTaskAttributionBase is the common base class of callback
// function that require task attribution support.
// Callbacks that require such a functionality should be defined with
// [SupportsTaskAttribution] IDL extended attribute.
class PLATFORM_EXPORT CallbackFunctionWithTaskAttributionBase
    : public CallbackFunctionBase {
 public:
  ~CallbackFunctionWithTaskAttributionBase() override = default;

  scheduler::TaskAttributionInfo* GetParentTask() const {
    return parent_task_.Get();
  }

  void SetParentTask(scheduler::TaskAttributionInfo* task) {
    parent_task_ = task;
  }

  void Trace(Visitor* visitor) const override {
    CallbackFunctionBase::Trace(visitor);
    visitor->Trace(parent_task_);
  }

 protected:
  explicit CallbackFunctionWithTaskAttributionBase(v8::Local<v8::Object> object)
      : CallbackFunctionBase(object) {}

 private:
  Member<scheduler::TaskAttributionInfo> parent_task_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_FUNCTION_BASE_H_
