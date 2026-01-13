// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_SUCCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_SUCCESS_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class LanguageModelToolResultContent;
class LanguageModelToolSuccessInit;

class MODULES_EXPORT LanguageModelToolSuccess final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static LanguageModelToolSuccess* Create(LanguageModelToolSuccessInit* init,
                                          ExceptionState& exception_state);

  LanguageModelToolSuccess(
      const String& call_id,
      const String& name,
      const HeapVector<Member<LanguageModelToolResultContent>>& result);

  // IDL attributes.
  const String& callID() const { return call_id_; }
  const String& name() const { return name_; }
  const HeapVector<Member<LanguageModelToolResultContent>>& result() const {
    return result_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  String call_id_;
  String name_;
  HeapVector<Member<LanguageModelToolResultContent>> result_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_TOOL_SUCCESS_H_
