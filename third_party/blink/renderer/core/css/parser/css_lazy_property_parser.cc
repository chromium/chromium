// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

namespace blink {

CSSLazyPropertyParser::CSSLazyPropertyParser(wtf_size_t offset,
                                             CSSLazyParsingState* state)
    : offset_(offset), lazy_state_(state) {}

CSSPropertyValueSet* CSSLazyPropertyParser::ParseProperties() {
  return CSSParserImpl::ParseDeclarationListForLazyStyle(
      lazy_state_->SheetText(), offset_, lazy_state_->Context());
}

void CSSLazyPropertyParser::Trace(Visitor* visitor) const {
  visitor->Trace(lazy_state_);
}

}  // namespace blink
