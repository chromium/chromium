// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
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

  // Sometimes we just check if the value matches specified type or syntax, but
  // we don't compute the value, for instance in attr() type(), hence we don't
  // actually need property name there.
  static CSSParserLocalContext CreateWithoutPropertyForSyntaxParsing() {
    return CSSParserLocalContext();
  }

  // TODO(crbug.com/488111037): This constructor is used for random() function
  // in ident(). Though we currently disallow random() inside ident()
  // parse-time, we need some dummy CSSParserLocalContext for parsing.
  static CSSParserLocalContext CreateWithoutPropertyForIdent() {
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
  // Since we don't support resolution of random() functions in DevTools for now
  // we don't need property context for random() in there.
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
  // `custom_function` parameter is used for values defined in CSS @function
  // arguments, result values and local variables.
  explicit CSSParserLocalContext(CSSPropertyName property_name,
                                 CSSPropertyID current_shorthand,
                                 const AtomicString& custom_function_name)
      : current_shorthand_(current_shorthand),
        unresolved_property_name_(property_name),
        custom_function_name_(custom_function_name) {}

  void SetCurrentShorthand(CSSPropertyID current_shorthand) {
    current_shorthand_ = current_shorthand;
  }

  void SetUnresolvedProperty(CSSPropertyName property_name) {
    unresolved_property_name_ = property_name;
  }

  void SetCustomFunctionName(const AtomicString& custom_function_name) {
    custom_function_name_ = custom_function_name;
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

  const AtomicString PropertyNameAndRandomCount() const;

  wtf_size_t RandomValueCount() const { return random_value_count_; }

  // We currently use this class to get the context for resolving percentages.
  // for instance `30%` in `color-mix(red 30%, white)` and in `translate(30%)`
  // means different things, while in `color-mix()` it shows the progress, in
  // `translate` percentages should be resolved against the reference box
  // dimensions. This is currently used for `random()` function.
  class FunctionLocalContext {
    STACK_ALLOCATED();

   public:
    FunctionLocalContext(CSSValueID function_id,
                         CSSParserLocalContext& local_context)
        : local_context_(local_context) {
      local_context_.functions_stack_.push_back(function_id);
    }
    ~FunctionLocalContext() {
      DCHECK(!local_context_.functions_stack_.empty());
      local_context_.functions_stack_.pop_back();
    }

   private:
    CSSParserLocalContext& local_context_;
  };

  // Checks whether percentages for the current property context depend on
  // used value and cannot be resolved before layout. We use this function to
  // determine if percentages in `random()` functions should be simplified
  // computed value time or should take the form of `random(fixed ...)`.
  bool PercentagesDependOnUsedValue() const;

  // All CSS properties that accept percentages should have
  // `percentages_depend_on_used_value` flag set in css_properties.json5. As
  // well as all CSS functions, like color-mix(), transform(), etc. that have
  // percentages in their syntax should define function local context, see
  // `CSSParserLocalContext::FunctionLocalContext` above.
#if DCHECK_IS_ON()
  void CheckPercentagesFlagSetOnProperty() const;
#endif

 private:
  bool InFunctionContext() const { return !functions_stack_.empty(); }

  CSSParserLocalContext() = default;

  CSSPropertyID current_shorthand_ = CSSPropertyID::kInvalid;
  std::optional<CSSPropertyName> unresolved_property_name_;
  HeapVector<CSSValueID> functions_stack_;

  // TODO(crbug.com/413385732): We might have the same function name between
  // different tree scopes, then we need to make CSSParserLocalContext aware
  // of tree scope name.
  AtomicString custom_function_name_ = g_null_atom;

  wtf_size_t random_value_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_LOCAL_CONTEXT_H_
