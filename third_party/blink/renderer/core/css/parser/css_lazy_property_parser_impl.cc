// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser_impl.h"

#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

namespace blink {

CSSLazyPropertyParserImpl::CSSLazyPropertyParserImpl(wtf_size_t offset,
                                                     CSSLazyParsingState* state)
    : CSSLazyPropertyParser(), offset_(offset), lazy_state_(state) {}

CSSPropertyValueSet* CSSLazyPropertyParserImpl::ParseProperties() {
  return CSSParserImpl::ParseDeclarationListForLazyStyle(
      lazy_state_->SheetText(), offset_, lazy_state_->Context());
}

}  // namespace blink
