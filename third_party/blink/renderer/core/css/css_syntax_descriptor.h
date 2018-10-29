// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_DESCRIPTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_DESCRIPTOR_H_

#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"

namespace blink {

class CSSParserContext;
class CSSValue;

class CORE_EXPORT CSSSyntaxDescriptor {
 public:
  explicit CSSSyntaxDescriptor(const String& syntax);

  const CSSValue* Parse(CSSParserTokenRange,
                        const CSSParserContext*,
                        bool is_animation_tainted) const;
  const CSSSyntaxComponent* Match(const CSSStyleValue&) const;
  bool CanTake(const CSSStyleValue&) const;
  bool IsValid() const { return !syntax_components_.IsEmpty(); }
  bool IsTokenStream() const {
    return syntax_components_.size() == 1 &&
           syntax_components_[0].GetType() == CSSSyntaxType::kTokenStream;
  }
  bool HasUrlSyntax() const {
    for (const CSSSyntaxComponent& component : syntax_components_) {
      if (component.GetType() == CSSSyntaxType::kUrl)
        return true;
    }
    return false;
  }
  const Vector<CSSSyntaxComponent>& Components() const {
    return syntax_components_;
  }
  bool operator==(const CSSSyntaxDescriptor& a) const {
    return Components() == a.Components();
  }
  bool operator!=(const CSSSyntaxDescriptor& a) const {
    return Components() != a.Components();
  }

 private:
  Vector<CSSSyntaxComponent> syntax_components_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_DESCRIPTOR_H_
