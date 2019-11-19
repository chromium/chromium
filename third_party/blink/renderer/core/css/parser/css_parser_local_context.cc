// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"

namespace blink {

CSSParserLocalContext::CSSParserLocalContext()
    : use_alias_parsing_(false),
      is_animation_tainted_(false),
      current_shorthand_(CSSPropertyID::kInvalid),
      variable_mode_(VariableMode::kTyped) {}

CSSParserLocalContext CSSParserLocalContext::WithAliasParsing(
    bool use_alias_parsing) const {
  CSSParserLocalContext context = *this;
  context.use_alias_parsing_ = use_alias_parsing;
  return context;
}

CSSParserLocalContext CSSParserLocalContext::WithAnimationTainted(
    bool is_animation_tainted) const {
  CSSParserLocalContext context = *this;
  context.is_animation_tainted_ = is_animation_tainted;
  return context;
}

CSSParserLocalContext CSSParserLocalContext::WithCurrentShorthand(
    CSSPropertyID current_shorthand) const {
  CSSParserLocalContext context = *this;
  context.current_shorthand_ = current_shorthand;
  return context;
}

CSSParserLocalContext CSSParserLocalContext::WithVariableMode(
    VariableMode variable_mode) const {
  CSSParserLocalContext context = *this;
  context.variable_mode_ = variable_mode;
  return context;
}

bool CSSParserLocalContext::UseAliasParsing() const {
  return use_alias_parsing_;
}

bool CSSParserLocalContext::IsAnimationTainted() const {
  return is_animation_tainted_;
}

CSSPropertyID CSSParserLocalContext::CurrentShorthand() const {
  return current_shorthand_;
}

CSSParserLocalContext::VariableMode CSSParserLocalContext::GetVariableMode()
    const {
  return variable_mode_;
}

}  // namespace blink
