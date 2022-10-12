// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MULTI_WORLDS_V8_REFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MULTI_WORLDS_V8_REFERENCE_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ScriptState;

class MultiWorldsV8Reference : public GarbageCollected<MultiWorldsV8Reference> {
 public:
  MultiWorldsV8Reference(v8::Isolate* isolate, v8::Local<v8::Value> value);

  v8::Local<v8::Value> GetValue(ScriptState* script_state);

  virtual void Trace(Visitor*) const;

 private:
  v8::Local<v8::Object> GetObject(ScriptState* script_state);

  TraceWrapperV8Reference<v8::Value> value_;
  Member<ScriptState> script_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MULTI_WORLDS_V8_REFERENCE_H_
