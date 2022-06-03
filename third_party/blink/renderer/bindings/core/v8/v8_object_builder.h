// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_OBJECT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_OBJECT_BUILDER_H_

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;
class ScriptValue;

class CORE_EXPORT V8ObjectBuilder final {
  STACK_ALLOCATED();

 public:
  explicit V8ObjectBuilder(ScriptState*);

  ScriptState* GetScriptState() const { return script_state_; }

  V8ObjectBuilder& Add(const StringView& name, const V8ObjectBuilder&);

  V8ObjectBuilder& AddNull(const StringView& name);
  V8ObjectBuilder& AddBoolean(const StringView& name, bool value);
  V8ObjectBuilder& AddNumber(const StringView& name, double value);
  V8ObjectBuilder& AddString(const StringView& name, const StringView& value);
  V8ObjectBuilder& AddStringOrNull(const StringView& name,
                                   const StringView& value);

  template <typename T>
  V8ObjectBuilder& Add(const StringView& name, const T& value) {
    AddInternal(name, v8::Local<v8::Value>(
                          ToV8(value, script_state_->GetContext()->Global(),
                               script_state_->GetIsolate())));
    return *this;
  }

  ScriptValue GetScriptValue() const;
  v8::Local<v8::Object> V8Value() const { return object_; }

 private:
  void AddInternal(const StringView& name, v8::Local<v8::Value>);

  ScriptState* script_state_;
  v8::Local<v8::Object> object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_OBJECT_BUILDER_H_
