// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_LONG_SEQUENCE_OR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_LONG_SEQUENCE_OR_EVENT_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Event;

class CORE_EXPORT LongSequenceOrEvent final {
  DISALLOW_NEW();
 public:
  LongSequenceOrEvent();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsEvent() const { return type_ == SpecificType::kEvent; }
  Event* GetAsEvent() const;
  void SetEvent(Event*);
  static LongSequenceOrEvent FromEvent(Event*);

  bool IsLongSequence() const { return type_ == SpecificType::kLongSequence; }
  const Vector<int32_t>& GetAsLongSequence() const;
  void SetLongSequence(const Vector<int32_t>&);
  static LongSequenceOrEvent FromLongSequence(const Vector<int32_t>&);

  LongSequenceOrEvent(const LongSequenceOrEvent&);
  ~LongSequenceOrEvent();
  LongSequenceOrEvent& operator=(const LongSequenceOrEvent&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kEvent,
    kLongSequence,
  };
  SpecificType type_;

  Member<Event> event_;
  Vector<int32_t> long_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const LongSequenceOrEvent&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8LongSequenceOrEvent final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, LongSequenceOrEvent&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const LongSequenceOrEvent&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, LongSequenceOrEvent& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, LongSequenceOrEvent& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<LongSequenceOrEvent> : public NativeValueTraitsBase<LongSequenceOrEvent> {
  CORE_EXPORT static LongSequenceOrEvent NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static LongSequenceOrEvent NullValue() { return LongSequenceOrEvent(); }
};

template <>
struct V8TypeOf<LongSequenceOrEvent> {
  typedef V8LongSequenceOrEvent Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::LongSequenceOrEvent);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_LONG_SEQUENCE_OR_EVENT_H_
