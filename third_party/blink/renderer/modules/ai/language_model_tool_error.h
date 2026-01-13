// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_ERROR_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class LanguageModelToolErrorInit;

class MODULES_EXPORT LanguageModelToolError final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static LanguageModelToolError* Create(LanguageModelToolErrorInit* init,
                                        ExceptionState& exception_state);

  LanguageModelToolError(const String& call_id,
                         const String& name,
                         const String& error_message);

  // IDL attributes.
  const String& callID() const { return call_id_; }
  const String& name() const { return name_; }
  const String& errorMessage() const { return error_message_; }

  void Trace(Visitor* visitor) const override;

 private:
  String call_id_;
  String name_;
  String error_message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_ERROR_H_
