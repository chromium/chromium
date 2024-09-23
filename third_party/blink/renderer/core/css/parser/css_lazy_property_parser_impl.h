// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_LAZY_PROPERTY_PARSER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_LAZY_PROPERTY_PARSER_IMPL_H_

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"

namespace blink {

class CSSLazyParsingState;

// This class is responsible for lazily parsing a single CSS declaration list.
class CSSLazyPropertyParserImpl : public CSSLazyPropertyParser {
 public:
  CSSLazyPropertyParserImpl(wtf_size_t offset, CSSLazyParsingState*);

  // CSSLazyPropertyParser:
  CSSPropertyValueSet* ParseProperties() override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(lazy_state_);
    CSSLazyPropertyParser::Trace(visitor);
  }

 private:
  wtf_size_t offset_;
  Member<CSSLazyParsingState> lazy_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_LAZY_PROPERTY_PARSER_IMPL_H_
