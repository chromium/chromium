// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_IF_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_IF_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/if_test.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

namespace blink {

class CORE_EXPORT CSSIfParser {
  STACK_ALLOCATED();

 public:
  explicit CSSIfParser(const CSSParserContext&);

  // Used for inline if() function. Supports only style() and media() queries in
  // condition for now. https://drafts.csswg.org/css-values-5/#if-notation
  std::optional<IfTest> ConsumeIfTest(CSSParserTokenStream&);

 private:
  ContainerQueryParser container_query_parser_;
  MediaQueryParser media_query_parser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_IF_PARSER_H_
