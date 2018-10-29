/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_VALUE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/shared_persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class CORE_EXPORT ScriptValue final {
  DISALLOW_NEW();

 public:
  // Defined in ToV8.h due to circular dependency
  template <typename T>
  static ScriptValue From(ScriptState*, T&& value);

  template <typename T, typename... Arguments>
  static inline T To(v8::Isolate* isolate,
                     v8::Local<v8::Value> value,
                     ExceptionState& exception_state,
                     Arguments const&... arguments) {
    return NativeValueTraits<T>::NativeValue(isolate, value, exception_state,
                                             arguments...);
  }

  template <typename T, typename... Arguments>
  static inline T To(v8::Isolate* isolate,
                     const ScriptValue& value,
                     ExceptionState& exception_state,
                     Arguments const&... arguments) {
    return To<T>(isolate, value.V8Value(), exception_state, arguments...);
  }

  ScriptValue() = default;

  ScriptValue(ScriptState* script_state, v8::Local<v8::Value> value)
      : script_state_(script_state),
        value_(value.IsEmpty() ? nullptr
                               : SharedPersistent<v8::Value>::Create(
                                     value,
                                     script_state->GetIsolate())) {
    DCHECK(IsEmpty() || script_state_);
  }

  template <typename T>
  ScriptValue(ScriptState* script_state, v8::MaybeLocal<T> value)
      : script_state_(script_state),
        value_(value.IsEmpty() ? nullptr
                               : SharedPersistent<v8::Value>::Create(
                                     value.ToLocalChecked(),
                                     script_state->GetIsolate())) {
    DCHECK(IsEmpty() || script_state_);
  }

  ScriptValue(const ScriptValue& value)
      : script_state_(value.script_state_), value_(value.value_) {
    DCHECK(IsEmpty() || script_state_);
  }

  ScriptState* GetScriptState() const { return script_state_; }

  v8::Isolate* GetIsolate() const {
    return script_state_ ? script_state_->GetIsolate()
                         : v8::Isolate::GetCurrent();
  }

  v8::Local<v8::Context> GetContext() const {
    DCHECK(script_state_);
    return script_state_->GetContext();
  }

  ScriptValue& operator=(const ScriptValue& value) {
    if (this != &value) {
      script_state_ = value.script_state_;
      value_ = value.value_;
    }
    return *this;
  }

  bool operator==(const ScriptValue& value) const {
    if (IsEmpty())
      return value.IsEmpty();
    if (value.IsEmpty())
      return false;
    return *value_ == *value.value_;
  }

  bool operator!=(const ScriptValue& value) const { return !operator==(value); }

  // This creates a new local Handle; Don't use this in performance-sensitive
  // places.
  bool IsFunction() const {
    DCHECK(!IsEmpty());
    v8::Local<v8::Value> value = V8Value();
    return !value.IsEmpty() && value->IsFunction();
  }

  // This creates a new local Handle; Don't use this in performance-sensitive
  // places.
  bool IsNull() const {
    DCHECK(!IsEmpty());
    v8::Local<v8::Value> value = V8Value();
    return !value.IsEmpty() && value->IsNull();
  }

  // This creates a new local Handle; Don't use this in performance-sensitive
  // places.
  bool IsUndefined() const {
    DCHECK(!IsEmpty());
    v8::Local<v8::Value> value = V8Value();
    return !value.IsEmpty() && value->IsUndefined();
  }

  // This creates a new local Handle; Don't use this in performance-sensitive
  // places.
  bool IsObject() const {
    DCHECK(!IsEmpty());
    v8::Local<v8::Value> value = V8Value();
    return !value.IsEmpty() && value->IsObject();
  }

  bool IsEmpty() const { return !value_.get() || value_->IsEmpty(); }

  void Clear() { value_ = nullptr; }

  v8::Local<v8::Value> V8Value() const;
  // Returns v8Value() if a given ScriptState is the same as the
  // ScriptState which is associated with this ScriptValue. Otherwise
  // this "clones" the v8 value and returns it.
  v8::Local<v8::Value> V8ValueFor(ScriptState*) const;

  bool ToString(String&) const;

  static ScriptValue CreateNull(ScriptState*);

 private:
  // TODO(peria): Move ScriptValue to Oilpan heap.
  GC_PLUGIN_IGNORE("813731")
  Persistent<ScriptState> script_state_;
  scoped_refptr<SharedPersistent<v8::Value>> value_;
};

template <>
struct NativeValueTraits<ScriptValue>
    : public NativeValueTraitsBase<ScriptValue> {
  static inline ScriptValue NativeValue(v8::Isolate* isolate,
                                        v8::Local<v8::Value> value,
                                        ExceptionState& exception_state) {
    return ScriptValue(ScriptState::Current(isolate), value);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_VALUE_H_
