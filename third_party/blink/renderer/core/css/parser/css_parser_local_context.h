// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// A wrapper class containing all local context when parsing a property.

class CORE_EXPORT CSSParserLocalContext {
  STACK_ALLOCATED();

 public:
  // TODO(crbug.com/475807587): CSS Typed OM currently does not specify
  // random(), so we will use empty property name for it for now.
  static CSSParserLocalContext CreateWithoutPropertyForCSSOM() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/413385732): This constructor is used for substitution
  // functions like var(), attr(), if(), @function, etc. This should be removed
  // once we support property-dependent random inside them.
  static CSSParserLocalContext CreateWithoutPropertyForSubstitutions() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/413385732): We used this constructor to create a local
  // context for animations, i.e. in all css_parsing_utils::Consume* calls from
  // files inside `third_party/blink/renderer/core/animation/`. Figure out if we
  // actually need property context for random() in there.
  static CSSParserLocalContext CreateWithoutPropertyForAnimations() {
    return CSSParserLocalContext();
  }

  // Should only be used for testing.
  static CSSParserLocalContext CreateWithoutPropertyForTest() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/475808971): We use this constructor for at-rules and their
  // descriptors. We should probably use descriptor name as property name for
  // random and only use this constructor inside rule definition since we don't
  // have a property context in that case.
  static CSSParserLocalContext CreateWithoutPropertyForAtRules() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/475808971): Since @media don't have property context, we for
  // now will use empty property name when caching random() values without user
  // specified identifier. This behaviour is not defined in spec, there is an
  // open issue addressing that:
  // https://drafts.csswg.org/css-values-5/#issue-cd071f29
  static CSSParserLocalContext CreateWithoutPropertyForMediaQueries() {
    return CSSParserLocalContext();
  }

  // This is used for view-transition pseudo selectors, like
  // ::view-transition-new(),
  // https://drafts.csswg.org/css-view-transitions/#selectordef-view-transition
  // There is a chance we can have random() inside ident() in there, but it's
  // not yet clear whether ident() should be allowed inside pseudo selectors:
  // https://github.com/w3c/csswg-drafts/issues/12219.
  static CSSParserLocalContext CreateWithoutPropertyForSelectors() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/413385732): This is used for parsing colors without element
  // context in `blink/renderer/modules/canvas/canvas2d/canvas_style.cc`. We
  // don't have property context there, so will use empty string as property
  // name for property-dependent random() values. We might want to disallow
  // random() at parse time for setting values on the canvas contexts.
  static CSSParserLocalContext CreateWithoutPropertyForCanvas() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/40068516): We use this constructor to create a local
  // context for all css_parsing_utils::Consume* calls from inspector classes.
  // Figure out if we actually need property context for random() in there.
  static CSSParserLocalContext CreateWithoutPropertyForInspector() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/413385732): Used to create a local context to parse input
  // arguments against syntax for CSSPaintValue. Figure out if we actually need
  // the property context for property dependent random() values there.
  static CSSParserLocalContext CreateWithoutPropertyForPaintValue() {
    return CSSParserLocalContext();
  }

  // For standard CSS properties, need to pass CSSPropertyName with unresolved
  // property id.
  explicit CSSParserLocalContext(CSSPropertyName property_name)
      : unresolved_property_name_(property_name) {}

  CSSParserLocalContext WithCurrentShorthand(
      CSSPropertyID current_shorthand) const {
    CSSParserLocalContext context = *this;
    context.current_shorthand_ = current_shorthand;
    return context;
  }

  void IncrementRandomValueCount() { ++random_value_count_; }

  bool UseAliasParsing() const {
    if (!unresolved_property_name_) {
      return false;
    }
    if (unresolved_property_name_->IsCustomProperty()) {
      return false;
    }
    return IsPropertyAlias(unresolved_property_name_->Id());
  }

  CSSPropertyID CurrentShorthand() const { return current_shorthand_; }

  std::optional<CSSPropertyName> UnresolvedPropertyName() const {
    return unresolved_property_name_;
  }

  const AtomicString PropertyNameAndRandomCount() const {
    StringBuilder str;
    if (unresolved_property_name_.has_value() &&
        unresolved_property_name_->Id() != CSSPropertyID::kInvalid) {
      // Use string of form "PROPERTY {property_name} {property_value_index}"
      // as name, this is later used for caching random values [0]. The prefix
      // "PROPERTY" is needed since we need to make distinguish between custom
      // property name and random value identifier, i.e. <dashed-ident> value in
      // <random-value-sharing> [1]
      // [0] https://drafts.csswg.org/css-values-5/#random-caching-key
      // [1] https://drafts.csswg.org/css-values-5/#typedef-random-value-sharing
      str.Append("PROPERTY ");
      CSSPropertyName resolved_property_name =
          unresolved_property_name_->IsCustomProperty()
              ? *unresolved_property_name_
              : CSSPropertyName(
                    ResolveCSSPropertyID(unresolved_property_name_->Id()));
      str.Append(resolved_property_name.ToAtomicString());
      str.Append(" ");
      str.AppendNumber(random_value_count_);
    }
    return str.ToAtomicString();
  }

  wtf_size_t RandomValueCount() const { return random_value_count_; }

 private:
  CSSParserLocalContext() = default;

  CSSPropertyID current_shorthand_ = CSSPropertyID::kInvalid;
  std::optional<CSSPropertyName> unresolved_property_name_;
  wtf_size_t random_value_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
