// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A wrapper class containing all local context when parsing a property.

class CORE_EXPORT CSSParserLocalContext {
  STACK_ALLOCATED();

 public:
  CSSParserLocalContext();

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

  CSSParserLocalContext WithAliasParsing(bool) const;
  CSSParserLocalContext WithAnimationTainted(bool) const;
  CSSParserLocalContext WithCurrentShorthand(CSSPropertyID) const;
  CSSParserLocalContext WithVariableMode(VariableMode) const;

  bool UseAliasParsing() const;
  // Any custom property used in a @keyframes rule becomes animation-tainted,
  // which prevents the custom property from being substituted into the
  // 'animation' property, or one of its longhands.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  bool IsAnimationTainted() const;
  CSSPropertyID CurrentShorthand() const;
  VariableMode GetVariableMode() const;

 private:
  bool use_alias_parsing_;
  bool is_animation_tainted_;
  CSSPropertyID current_shorthand_;
  VariableMode variable_mode_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
