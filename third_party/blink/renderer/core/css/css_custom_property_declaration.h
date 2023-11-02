// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSParserContext;

class CORE_EXPORT CSSCustomPropertyDeclaration : public CSSValue {
 public:
  CSSCustomPropertyDeclaration(scoped_refptr<CSSVariableData> value,
                               const CSSParserContext* parser_context)
      : CSSValue(kCustomPropertyDeclarationClass),
        value_(std::move(value)),
        parser_context_(parser_context) {
    DCHECK(value_);
  }

  CSSVariableData& Value() const { return *value_; }
  const CSSParserContext* ParserContext() const {
    return parser_context_.Get();
  }

  String CustomCSSText() const;

  bool Equals(const CSSCustomPropertyDeclaration& other) const {
    return this == &other;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  scoped_refptr<CSSVariableData> value_;

  // The parser context is used to resolve relative URLs, as described in:
  // https://drafts.css-houdini.org/css-properties-values-api-1/#relative-urls
  Member<const CSSParserContext> parser_context_;
};

template <>
struct DowncastTraits<CSSCustomPropertyDeclaration> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsCustomPropertyDeclaration();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CUSTOM_PROPERTY_DECLARATION_H_
