// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/conditional_parser.h"

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

using css_parsing_utils::AtIdent;
using css_parsing_utils::ConsumeAnyValue;
using css_parsing_utils::ConsumeIfIdent;

const ConditionalExpNode* ConditionalParser::ConsumeCondition(
    CSSParserTokenStream& stream,
    ParseMode parse_mode) {
  if (ConsumeIfIdent(stream, "not")) {
    return ConditionalExpNode::Not(ConsumeInner(stream));
  }

  const ConditionalExpNode* result = ConsumeInner(stream);

  if (AtIdent(stream.Peek(), "and")) {
    while (result && ConsumeIfIdent(stream, "and")) {
      result = ConditionalExpNode::And(result, ConsumeInner(stream));
    }
  } else if (result && AtIdent(stream.Peek(), "or") &&
             parse_mode != ParseMode::kWithoutOr) {
    while (result && ConsumeIfIdent(stream, "or")) {
      result = ConditionalExpNode::Or(result, ConsumeInner(stream));
    }
  }

  return result;
}

const ConditionalExpNode* ConditionalParser::ConsumeInner(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    CSSParserTokenStream::State savepoint = stream.Save();
    const ConditionalExpNode* node = ConsumeLeaf(stream);
    if (!node) {
      stream.Restore(savepoint);
      node = ConsumeCondition(stream);
    }
    if (node && guard.Release()) {
      stream.ConsumeWhitespace();
      return ConditionalExpNode::Nested(node);
    }
  } else if (stream.Peek().GetType() == kFunctionToken) {
    CSSParserTokenStream::State savepoint = stream.Save();
    if (const ConditionalExpNode* function = ConsumeFunction(stream)) {
      stream.ConsumeWhitespace();
      return function;
    }
    stream.Restore(savepoint);
  }

  return ConsumeGeneralEnclosed(stream);
}

const ConditionalExpNode* ConditionalParser::ConsumeGeneralEnclosed(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kLeftParenthesisToken &&
      stream.Peek().GetType() != kFunctionToken) {
    return nullptr;
  }

  wtf_size_t start_offset = stream.Offset();
  StringView general_enclosed;
  {
    CSSParserTokenStream::BlockGuard guard(stream);

    stream.ConsumeWhitespace();

    // Note that <any-value> is optional in <general-enclosed>, so having an
    // empty block is fine.
    ConsumeAnyValue(stream);
    if (!stream.AtEnd()) {
      return nullptr;
    }
  }

  wtf_size_t end_offset = stream.Offset();

  // TODO(crbug.com/40627130): This is not well specified.
  general_enclosed =
      stream.StringRangeAt(start_offset, end_offset - start_offset);

  stream.ConsumeWhitespace();
  return MakeGarbageCollected<ConditionalExpNodeUnknown>(
      general_enclosed.ToString());
}

}  // namespace blink
