// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_LINK_CONDITION_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_LINK_CONDITION_PARSER_H_

#include "base/memory/stack_allocated.h"

namespace blink {

class CSSParserTokenStream;
class Document;
class LinkCondition;

class LinkConditionParser {
  STACK_ALLOCATED();

 public:
  static LinkCondition* Parse(CSSParserTokenStream&, const Document&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_LINK_CONDITION_PARSER_H_
