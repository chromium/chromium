// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORLD_SAFE_V8_REFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORLD_SAFE_V8_REFERENCE_H_

#include "base/check.h"
#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

// This is a namespace to provide utility functions to WorldSafeV8Reference.
class CORE_EXPORT WorldSafeV8ReferenceInternal final {
  STATIC_ONLY(WorldSafeV8ReferenceInternal);

 private:
  // Returns a V8 reference that is safe to access in `target_script_state`.
  // The return value may be a cloned object.
  //
  // TODO(crbug.com/803478): ScriptValue::V8ValueFor should be replaced with
  // this function, or these two functions should be merged at least.
  static v8::Local<v8::Value> ToWorldSafeValue(
      ScriptState* target_script_state,
      const TraceWrapperV8Reference<v8::Value>& v8_reference,
      ScriptState* source_script_state);

  // Checks the world of |value|'s creation context if |value| is a v8::Object.
  // The given |world| and |value|'s world must match.  Otherwise, crashes.
  //
  // TODO(yukishiino): Find the best place to put this function.  We might want
  // to share this function among other clients, e.g. access to wrapper objects
  // across worlds.
  static void MaybeCheckCreationContext(
      v8::Isolate*,
      const v8::Local<v8::Context> current_context,
      const DOMWrapperWorld& current_world,
      const v8::Local<v8::Value> value);

  template <typename V8Type>
  friend class WorldSafeV8Reference;
};

// This class provides safe access to v8::Value across worlds.  This class
// provides accessors that check whether the value is accessed in the same world
// or not, also provides an accessor that clones the value when accessed across
// worlds.
//
// TODO(crbug.com/1008765): Allow WorldSafeV8Reference created/set not in
// context.
template <typename V8Type>
class WorldSafeV8Reference final {
  DISALLOW_NEW();

 public:
  WorldSafeV8Reference() = default;

  WorldSafeV8Reference(v8::Isolate* isolate, v8::Local<V8Type> value)
      : v8_reference_(isolate, value) {
    if (value.IsEmpty()) {
      return;
    }

    DCHECK(isolate);
    // `script_state_` records the current realm when this V8 reference is
    // created. When created outside of any V8 context and `value` is an
    // object, fall back to `value`'s relevant realm so `value->IsObject()`
    // always implies a non-null `script_state_`.
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    if (!current_context.IsEmpty()) [[likely]] {
      ScriptState* script_state = ScriptState::From(isolate, current_context);
      script_state_ = script_state;
      WorldSafeV8ReferenceInternal::MaybeCheckCreationContext(
          isolate, current_context, script_state->World(), value);
    } else if (value->IsObject()) {
      ScriptState* script_state = ScriptState::ForRelevantRealm(
          isolate, value.template As<v8::Object>());
      CHECK(script_state);
      script_state_ = script_state;
    }
  }

  ~WorldSafeV8Reference() = default;

  // Returns the V8 reference.  Crashes if `script_state_` is set and its world
  // is different from `target_script_state`'s world.
  v8::Local<V8Type> Get(ScriptState* target_script_state) const {
    DCHECK(!v8_reference_.IsEmpty());
    v8::Local<V8Type> value =
        v8_reference_.Get(target_script_state->GetIsolate());
    if (script_state_) [[likely]] {
      CHECK_EQ(&script_state_->World(), &target_script_state->World());
    } else {
      CHECK(!value->IsObject());
    }
    return value;
  }

  // Returns a V8 reference that is safe to access in `target_script_state`.
  // The return value may be a cloned object.
  v8::Local<V8Type> GetAcrossWorld(ScriptState* target_script_state) const {
    return WorldSafeV8ReferenceInternal::ToWorldSafeValue(
               target_script_state, v8_reference_, script_state_.Get())
        .template As<V8Type>();
  }

  // Sets a new V8 reference.  Crashes if `script_state_` is set and its world
  // is different from `new_value`'s world.
  void Set(v8::Isolate* isolate, v8::Local<V8Type> new_value) {
    DCHECK(!new_value.IsEmpty());
    CHECK(isolate->InContext());
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    ScriptState* new_script_state = ScriptState::From(isolate, current_context);
    const DOMWrapperWorld& new_world = new_script_state->World();
    WorldSafeV8ReferenceInternal::MaybeCheckCreationContext(
        isolate, current_context, new_world, new_value);
    CHECK(v8_reference_.IsEmpty() ||
          (script_state_ && &script_state_->World() == &new_world));
    v8_reference_.Reset(isolate, new_value);
    script_state_ = new_script_state;
  }

  // Forcibly sets a new V8 reference even when the worlds are different.  The
  // recorded realm of this V8 reference will be updated to the current realm.
  void SetAcrossWorld(v8::Isolate* isolate, v8::Local<V8Type> new_value) {
    DCHECK(!new_value.IsEmpty());
    CHECK(isolate->InContext());
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    ScriptState* new_script_state = ScriptState::From(isolate, current_context);
    WorldSafeV8ReferenceInternal::MaybeCheckCreationContext(
        isolate, current_context, new_script_state->World(), new_value);
    v8_reference_.Reset(isolate, new_value);
    script_state_ = new_script_state;
  }

  void Reset() {
    v8_reference_.Reset();
    script_state_.Clear();
  }

  bool IsEmpty() const { return v8_reference_.IsEmpty(); }

  v8::Isolate* GetIsolate() const {
    DCHECK(!IsEmpty());
    DCHECK(script_state_);
    return script_state_->GetIsolate();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(v8_reference_);
    visitor->Trace(script_state_);
  }

  WorldSafeV8Reference& operator=(const WorldSafeV8Reference<V8Type>& other) =
      default;

  bool operator==(const WorldSafeV8Reference<V8Type>& other) const {
    return v8_reference_ == other.v8_reference_;
  }

 private:
  TraceWrapperV8Reference<V8Type> v8_reference_;
  // The `ScriptState` of the current realm at the time `v8_reference_` was set
  // (or `v8_reference_`'s relevant realm if constructed out-of-context with an
  // object). Never exposed to callers; used strictly to enforce world matching
  // in `Get()` and to run structured serialization in the source realm inside
  // `ToWorldSafeValue()` before deserializing into the destination realm.
  Member<ScriptState> script_state_;
};

template <typename V8Type>
struct VectorTraits<WorldSafeV8Reference<V8Type>>
    : VectorTraitsBase<WorldSafeV8Reference<V8Type>> {
  STATIC_ONLY(VectorTraits);

  static constexpr bool kCanInitializeWithMemset =
      VectorTraits<TraceWrapperV8Reference<V8Type>>::kCanInitializeWithMemset &&
      VectorTraits<Member<ScriptState>>::kCanInitializeWithMemset;
  static constexpr bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<
          TraceWrapperV8Reference<V8Type>>::kCanClearUnusedSlotsWithMemset &&
      VectorTraits<Member<ScriptState>>::kCanClearUnusedSlotsWithMemset;
  static constexpr bool kCanCopyWithMemcpy =
      VectorTraits<TraceWrapperV8Reference<V8Type>>::kCanCopyWithMemcpy &&
      VectorTraits<Member<ScriptState>>::kCanCopyWithMemcpy;
  static constexpr bool kCanMoveWithMemcpy =
      VectorTraits<TraceWrapperV8Reference<V8Type>>::kCanMoveWithMemcpy &&
      VectorTraits<Member<ScriptState>>::kCanMoveWithMemcpy;

  static constexpr bool kCanTraceConcurrently =
      VectorTraits<TraceWrapperV8Reference<V8Type>>::kCanTraceConcurrently &&
      VectorTraits<Member<ScriptState>>::kCanTraceConcurrently;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORLD_SAFE_V8_REFERENCE_H_
