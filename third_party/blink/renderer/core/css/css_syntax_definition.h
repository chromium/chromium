// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_DEFINITION_H_

#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"

namespace blink {

class CSSParserContext;
class CSSValue;

class CORE_EXPORT CSSSyntaxDefinition {
 public:
  const CSSValue* Parse(CSSParserTokenRange,
                        const CSSParserContext*,
                        bool is_animation_tainted) const;
  bool IsTokenStream() const {
    return syntax_components_.size() == 1 &&
           syntax_components_[0].GetType() == CSSSyntaxType::kTokenStream;
  }
  const Vector<CSSSyntaxComponent>& Components() const {
    return syntax_components_;
  }
  bool operator==(const CSSSyntaxDefinition& a) const {
    return Components() == a.Components();
  }
  bool operator!=(const CSSSyntaxDefinition& a) const {
    return Components() != a.Components();
  }

  CSSSyntaxDefinition IsolatedCopy() const;

 private:
  friend class CSSSyntaxStringParser;
  friend class CSSSyntaxStringParserTest;

  explicit CSSSyntaxDefinition(Vector<CSSSyntaxComponent>);

  // https://drafts.css-houdini.org/css-properties-values-api-1/#universal-syntax-descriptor
  static CSSSyntaxDefinition CreateUniversal();

  Vector<CSSSyntaxComponent> syntax_components_;
};

}  // namespace blink

namespace WTF {

template <wtf_size_t inlineCapacity, typename Allocator>
struct CrossThreadCopier<
    Vector<blink::CSSSyntaxDefinition, inlineCapacity, Allocator>> {
  using Type = Vector<blink::CSSSyntaxDefinition, inlineCapacity, Allocator>;
  static Type Copy(const Type& value) {
    Type result;
    result.ReserveInitialCapacity(value.size());
    for (const auto& element : value)
      result.push_back(element.IsolatedCopy());
    return result;
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_DEFINITION_H_
