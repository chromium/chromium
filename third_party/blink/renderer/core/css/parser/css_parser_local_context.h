// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A wrapper class containing all local context when parsing a property.

class CORE_EXPORT CSSParserLocalContext {
  STACK_ALLOCATED();

 public:
  CSSParserLocalContext() = default;

  CSSParserLocalContext WithCurrentShorthand(
      CSSPropertyID current_shorthand) const {
    CSSParserLocalContext context = *this;
    context.current_shorthand_ = current_shorthand;
    return context;
  }

  // For non custom properties, need to pass CSSPropertyName with unresolved
  // property id.
  CSSParserLocalContext WithPropertyName(CSSPropertyName property_name) const {
    CSSParserLocalContext context = *this;
    context.property_name_ = property_name;
    return context;
  }

  void IncrementRandomValueCount() { ++random_value_count_; }

  bool UseAliasParsing() const {
    if (property_name_.IsCustomProperty()) {
      return false;
    }
    return IsPropertyAlias(property_name_.Id());
  }

  CSSPropertyID CurrentShorthand() const { return current_shorthand_; }

  CSSPropertyName PropertyName() const {
    if (property_name_.IsCustomProperty()) {
      return property_name_;
    }
    return CSSPropertyName(ResolveCSSPropertyID(property_name_.Id()));
  }

  wtf_size_t RandomValueCount() const { return random_value_count_; }

 private:
  CSSPropertyID current_shorthand_ = CSSPropertyID::kInvalid;
  CSSPropertyName property_name_ = CSSPropertyName(g_empty_atom);
  wtf_size_t random_value_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
