// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_CALL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_CALL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class LanguageModelToolCallInit;
class ScriptState;

class MODULES_EXPORT LanguageModelToolCall final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static LanguageModelToolCall* Create(LanguageModelToolCallInit* init,
                                       ExceptionState& exception_state);

  LanguageModelToolCall(const String& call_id,
                        const String& name,
                        ScriptValue arguments);

  // IDL attributes.
  const String& callID() const { return call_id_; }
  const String& name() const { return name_; }
  v8::Local<v8::Value> arguments(ScriptState* script_state) const;

  void Trace(Visitor* visitor) const override;

 private:
  String call_id_;
  String name_;
  ScriptValue arguments_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_CALL_H_
