// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A wrapper class containing all local context when parsing a property.

class CORE_EXPORT CSSParserLocalContext {
  STACK_ALLOCATED();

 public:
  CSSParserLocalContext() = default;

  // When parsing registered custom properties, a different result is required
  // depending on the context.
  enum class VariableMode {
    // The custom property is parsed according to the registered syntax (if
    // available).
    kTyped,
    // The registration of the custom property (if any) is ignored; the custom
    // property will parse as if unregistered.
    kUntyped,
    // The custom property will be parsed as if unregistered (that is,
    // a CSSCustomPropertyDeclaration will be returned), but the tokens must
    // also match the registered syntax (if any). This is useful for CSSOM,
    // where incoming values must validate against the registered syntax, but
    // are otherwise treated as unregistered.
    kValidatedUntyped
  };

  CSSParserLocalContext WithAliasParsing(bool use_alias_parsing) const {
    CSSParserLocalContext context = *this;
    context.use_alias_parsing_ = use_alias_parsing;
    return context;
  }

  CSSParserLocalContext WithAnimationTainted(bool is_animation_tainted) const {
    CSSParserLocalContext context = *this;
    context.is_animation_tainted_ = is_animation_tainted;
    return context;
  }

  CSSParserLocalContext WithCurrentShorthand(
      CSSPropertyID current_shorthand) const {
    CSSParserLocalContext context = *this;
    context.current_shorthand_ = current_shorthand;
    return context;
  }

  CSSParserLocalContext WithVariableMode(VariableMode variable_mode) const {
    CSSParserLocalContext context = *this;
    context.variable_mode_ = variable_mode;
    return context;
  }

  bool UseAliasParsing() const { return use_alias_parsing_; }

  // Any custom property used in a @keyframes rule becomes animation-tainted,
  // which prevents the custom property from being substituted into the
  // 'animation' property, or one of its longhands.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  bool IsAnimationTainted() const { return is_animation_tainted_; }

  CSSPropertyID CurrentShorthand() const { return current_shorthand_; }

  VariableMode GetVariableMode() const { return variable_mode_; }

 private:
  bool use_alias_parsing_ = false;
  bool is_animation_tainted_ = false;
  CSSPropertyID current_shorthand_ = CSSPropertyID::kInvalid;
  VariableMode variable_mode_ = VariableMode::kTyped;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
