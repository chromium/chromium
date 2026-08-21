// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_VARIABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_VARIABLE_H_

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSPrivateVariable final : public GarbageCollected<CSSPrivateVariable> {
 public:
  CSSPrivateVariable(AtomicString name,
                     CSSSyntaxDefinition syntax,
                     CSSVariableData* default_value,
                     const CSSParserContext* parser_context)
      : name_(std::move(name)),
        syntax_(std::move(syntax)),
        default_value_(default_value),
        parser_context_(parser_context) {}
  CSSPrivateVariable(const CSSPrivateVariable&) = default;

  const AtomicString& Name() const { return name_; }
  const CSSSyntaxDefinition& Syntax() const { return syntax_; }
  CSSVariableData* DefaultValue() const { return default_value_.Get(); }
  const CSSParserContext* ParserContext() const {
    return parser_context_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(default_value_);
    visitor->Trace(parser_context_);
  }

 private:
  const AtomicString name_;
  const CSSSyntaxDefinition syntax_;
  const Member<CSSVariableData> default_value_;
  const Member<const CSSParserContext> parser_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_VARIABLE_H_
