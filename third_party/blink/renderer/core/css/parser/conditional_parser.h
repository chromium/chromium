// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CONDITIONAL_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CONDITIONAL_PARSER_H_

namespace blink {

class CSSParserTokenStream;
class ConditionalExpNode;

// Common conditional parser used by @media queries, @container queries, if()
// functions, etc. Handles parentheses, "and", "or", and "not" operators. The
// rest is left to the concrete implementation of a parser through abstract
// functions.
class CORE_EXPORT ConditionalParser {
  STACK_ALLOCATED();

 public:
  enum class ParseMode {
    // https://drafts.csswg.org/mediaqueries-4/#typedef-media-condition
    kNormal,
    // https://drafts.csswg.org/mediaqueries-4/#typedef-media-condition-without-or
    kWithoutOr,
  };

  // Parser entrypoint. Return the root expression node, or nullptr if parsing
  // failed.
  const ConditionalExpNode* ConsumeCondition(CSSParserTokenStream&,
                                             ParseMode = ParseMode::kNormal);

 private:
  const ConditionalExpNode* ConsumeInner(CSSParserTokenStream&);
  const ConditionalExpNode* ConsumeGeneralEnclosed(CSSParserTokenStream&);

  // Consume a feature-specific expression (e.g. "width > 700px", for media
  // queries), and return the resulting expression node. The implementation
  // should not wrap everything inside a nested expression node, since the
  // caller will take care of that.
  //
  // The caller takes care of leaving the parser offset unchanged if nullptr is
  // returned.
  virtual const ConditionalExpNode* ConsumeLeaf(CSSParserTokenStream&) = 0;

  // Consume a feature-specific function and return the resulting expression
  // node upon success, otherwise nullptr. If successful, the caller will also
  // consume any trailing whitespace.
  //
  // The caller takes care of leaving the parser offset unchanged if nullptr is
  // returned.
  virtual const ConditionalExpNode* ConsumeFunction(CSSParserTokenStream&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CONDITIONAL_PARSER_H_
