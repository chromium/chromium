/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/style_rule.h"

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/css/css_apply_mixin_rule.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_contents_mixin_rule.h"
#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/css_custom_media_rule.h"
#include "third_party/blink/renderer/core/css/css_font_face_rule.h"
#include "third_party/blink/renderer/core/css/css_font_feature_values_rule.h"
#include "third_party/blink/renderer/core/css/css_font_palette_values_rule.h"
#include "third_party/blink/renderer/core/css/css_function_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_layer_block_rule.h"
#include "third_party/blink/renderer/core/css/css_layer_statement_rule.h"
#include "third_party/blink/renderer/core/css/css_margin_rule.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_mixin_rule.h"
#include "third_party/blink/renderer/core/css/css_namespace_rule.h"
#include "third_party/blink/renderer/core/css/css_navigation_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_page_rule.h"
#include "third_party/blink/renderer/core/css/css_position_try_rule.h"
#include "third_party/blink/renderer/core/css/css_property_rule.h"
#include "third_party/blink/renderer/core/css/css_route_rule.h"
#include "third_party/blink/renderer/core/css/css_scope_rule.h"
#include "third_party/blink/renderer/core/css/css_starting_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/css_view_transition_rule.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_function_declarations.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"
#include "third_party/blink/renderer/core/css/style_rule_route.h"
#include "third_party/blink/renderer/core/css/style_rule_view_transition.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsStyleRuleBase final
    : public GarbageCollected<SameSizeAsStyleRuleBase> {
  uint8_t field;
};

ASSERT_SIZE(StyleRuleBase, SameSizeAsStyleRuleBase);

CSSRule* StyleRuleBase::CreateCSSOMWrapper(wtf_size_t position_hint,
                                           CSSStyleSheet* parent_sheet,
                                           bool trigger_use_counters) const {
  return CreateCSSOMWrapper(position_hint, parent_sheet, nullptr,
                            trigger_use_counters);
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(wtf_size_t position_hint,
                                           CSSRule* parent_rule,
                                           bool trigger_use_counters) const {
  return CreateCSSOMWrapper(position_hint, nullptr, parent_rule,
                            trigger_use_counters);
}

void StyleRuleBase::Trace(Visitor* visitor) const {
  switch (GetType()) {
    case kCharset:
      To<StyleRuleCharset>(this)->TraceAfterDispatch(visitor);
      return;
    case kStyle:
      To<StyleRule>(this)->TraceAfterDispatch(visitor);
      return;
    case kPage:
      To<StyleRulePage>(this)->TraceAfterDispatch(visitor);
      return;
    case kPageMargin:
      To<StyleRulePageMargin>(this)->TraceAfterDispatch(visitor);
      return;
    case kProperty:
      To<StyleRuleProperty>(this)->TraceAfterDispatch(visitor);
      return;
    case kRoute:
      To<StyleRuleRoute>(this)->TraceAfterDispatch(visitor);
      return;
    case kNavigation:
      To<StyleRuleNavigation>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFace:
      To<StyleRuleFontFace>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontPaletteValues:
      To<StyleRuleFontPaletteValues>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFeatureValues:
      To<StyleRuleFontFeatureValues>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFeature:
      To<StyleRuleFontFeature>(this)->TraceAfterDispatch(visitor);
      return;
    case kMedia:
      To<StyleRuleMedia>(this)->TraceAfterDispatch(visitor);
      return;
    case kNestedDeclarations:
      To<StyleRuleNestedDeclarations>(this)->TraceAfterDispatch(visitor);
      return;
    case kFunctionDeclarations:
      To<StyleRuleFunctionDeclarations>(this)->TraceAfterDispatch(visitor);
      return;
    case kScope:
      To<StyleRuleScope>(this)->TraceAfterDispatch(visitor);
      return;
    case kSupports:
      To<StyleRuleSupports>(this)->TraceAfterDispatch(visitor);
      return;
    case kImport:
      To<StyleRuleImport>(this)->TraceAfterDispatch(visitor);
      return;
    case kKeyframes:
      To<StyleRuleKeyframes>(this)->TraceAfterDispatch(visitor);
      return;
    case kKeyframe:
      To<StyleRuleKeyframe>(this)->TraceAfterDispatch(visitor);
      return;
    case kLayerBlock:
      To<StyleRuleLayerBlock>(this)->TraceAfterDispatch(visitor);
      return;
    case kLayerStatement:
      To<StyleRuleLayerStatement>(this)->TraceAfterDispatch(visitor);
      return;
    case kNamespace:
      To<StyleRuleNamespace>(this)->TraceAfterDispatch(visitor);
      return;
    case kContainer:
      To<StyleRuleContainer>(this)->TraceAfterDispatch(visitor);
      return;
    case kCounterStyle:
      To<StyleRuleCounterStyle>(this)->TraceAfterDispatch(visitor);
      return;
    case kStartingStyle:
      To<StyleRuleStartingStyle>(this)->TraceAfterDispatch(visitor);
      return;
    case kViewTransition:
      To<StyleRuleViewTransition>(this)->TraceAfterDispatch(visitor);
      return;
    case kFunction:
      To<StyleRuleFunction>(this)->TraceAfterDispatch(visitor);
      return;
    case kMixin:
      To<StyleRuleMixin>(this)->TraceAfterDispatch(visitor);
      return;
    case kApplyMixin:
      To<StyleRuleApplyMixin>(this)->TraceAfterDispatch(visitor);
      return;
    case kContents:
      To<StyleRuleContentsStatement>(this)->TraceAfterDispatch(visitor);
      return;
    case kPositionTry:
      To<StyleRulePositionTry>(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomMedia:
      To<StyleRuleCustomMedia>(this)->TraceAfterDispatch(visitor);
      return;
  }
  DUMP_WILL_BE_NOTREACHED();
}

void StyleRuleBase::FinalizeGarbageCollectedObject() {
  switch (GetType()) {
    case kCharset:
      To<StyleRuleCharset>(this)->~StyleRuleCharset();
      return;
    case kStyle:
      To<StyleRule>(this)->~StyleRule();
      return;
    case kPage:
      To<StyleRulePage>(this)->~StyleRulePage();
      return;
    case kPageMargin:
      To<StyleRulePageMargin>(this)->~StyleRulePageMargin();
      return;
    case kProperty:
      To<StyleRuleProperty>(this)->~StyleRuleProperty();
      return;
    case kRoute:
      To<StyleRuleRoute>(this)->~StyleRuleRoute();
      return;
    case kNavigation:
      To<StyleRuleNavigation>(this)->~StyleRuleNavigation();
      return;
    case kFontFace:
      To<StyleRuleFontFace>(this)->~StyleRuleFontFace();
      return;
    case kFontPaletteValues:
      To<StyleRuleFontPaletteValues>(this)->~StyleRuleFontPaletteValues();
      return;
    case kFontFeatureValues:
      To<StyleRuleFontFeatureValues>(this)->~StyleRuleFontFeatureValues();
      return;
    case kFontFeature:
      To<StyleRuleFontFeature>(this)->~StyleRuleFontFeature();
      return;
    case kMedia:
      To<StyleRuleMedia>(this)->~StyleRuleMedia();
      return;
    case kNestedDeclarations:
      To<StyleRuleNestedDeclarations>(this)->~StyleRuleNestedDeclarations();
      return;
    case kFunctionDeclarations:
      To<StyleRuleFunctionDeclarations>(this)->~StyleRuleFunctionDeclarations();
      return;
    case kScope:
      To<StyleRuleScope>(this)->~StyleRuleScope();
      return;
    case kSupports:
      To<StyleRuleSupports>(this)->~StyleRuleSupports();
      return;
    case kImport:
      To<StyleRuleImport>(this)->~StyleRuleImport();
      return;
    case kKeyframes:
      To<StyleRuleKeyframes>(this)->~StyleRuleKeyframes();
      return;
    case kKeyframe:
      To<StyleRuleKeyframe>(this)->~StyleRuleKeyframe();
      return;
    case kLayerBlock:
      To<StyleRuleLayerBlock>(this)->~StyleRuleLayerBlock();
      return;
    case kLayerStatement:
      To<StyleRuleLayerStatement>(this)->~StyleRuleLayerStatement();
      return;
    case kNamespace:
      To<StyleRuleNamespace>(this)->~StyleRuleNamespace();
      return;
    case kContainer:
      To<StyleRuleContainer>(this)->~StyleRuleContainer();
      return;
    case kCounterStyle:
      To<StyleRuleCounterStyle>(this)->~StyleRuleCounterStyle();
      return;
    case kStartingStyle:
      To<StyleRuleStartingStyle>(this)->~StyleRuleStartingStyle();
      return;
    case kViewTransition:
      To<StyleRuleViewTransition>(this)->~StyleRuleViewTransition();
      return;
    case kFunction:
      To<StyleRuleFunction>(this)->~StyleRuleFunction();
      return;
    case kMixin:
      To<StyleRuleMixin>(this)->~StyleRuleMixin();
      return;
    case kApplyMixin:
      To<StyleRuleApplyMixin>(this)->~StyleRuleApplyMixin();
      return;
    case kContents:
      To<StyleRuleContentsStatement>(this)->~StyleRuleContentsStatement();
      return;
    case kPositionTry:
      To<StyleRulePositionTry>(this)->~StyleRulePositionTry();
      return;
    case kCustomMedia:
      To<StyleRuleCustomMedia>(this)->~StyleRuleCustomMedia();
      return;
  }
  NOTREACHED();
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(wtf_size_t position_hint,
                                           CSSStyleSheet* parent_sheet,
                                           CSSRule* parent_rule,
                                           bool trigger_use_counters) const {
  CSSRule* rule = nullptr;
  StyleRuleBase* self = const_cast<StyleRuleBase*>(this);
  switch (GetType()) {
    case kStyle:
      rule = MakeGarbageCollected<CSSStyleRule>(To<StyleRule>(self),
                                                parent_sheet, position_hint);
      break;
    case kPage:
      if (trigger_use_counters && parent_sheet) {
        UseCounter::Count(parent_sheet->OwnerDocument(),
                          WebFeature::kCSSPageRule);
      }
      rule = MakeGarbageCollected<CSSPageRule>(To<StyleRulePage>(self),
                                               parent_sheet);
      break;
    case kPageMargin:
      rule = MakeGarbageCollected<CSSMarginRule>(To<StyleRulePageMargin>(self),
                                                 parent_sheet);
      break;
    case kRoute:
      rule = MakeGarbageCollected<CSSRouteRule>(To<StyleRuleRoute>(self),
                                                parent_sheet);
      break;
    case kNavigation:
      rule = MakeGarbageCollected<CSSNavigationRule>(
          To<StyleRuleNavigation>(self), parent_sheet);
      break;
    case kProperty:
      rule = MakeGarbageCollected<CSSPropertyRule>(To<StyleRuleProperty>(self),
                                                   parent_sheet);
      break;
    case kFontFace:
      rule = MakeGarbageCollected<CSSFontFaceRule>(To<StyleRuleFontFace>(self),
                                                   parent_sheet);
      break;
    case kFontPaletteValues:
      rule = MakeGarbageCollected<CSSFontPaletteValuesRule>(
          To<StyleRuleFontPaletteValues>(self), parent_sheet);
      break;
    case kFontFeatureValues:
      rule = MakeGarbageCollected<CSSFontFeatureValuesRule>(
          To<StyleRuleFontFeatureValues>(self), parent_sheet);
      break;
    case kMedia:
      rule = MakeGarbageCollected<CSSMediaRule>(To<StyleRuleMedia>(self),
                                                parent_sheet);
      break;
    case kNestedDeclarations:
      rule = MakeGarbageCollected<CSSNestedDeclarationsRule>(
          To<StyleRuleNestedDeclarations>(self), parent_sheet);
      break;
    case kFunctionDeclarations:
      rule = MakeGarbageCollected<CSSFunctionDeclarationsRule>(
          To<StyleRuleFunctionDeclarations>(self), parent_sheet);
      break;
    case kFunction:
      rule = MakeGarbageCollected<CSSFunctionRule>(To<StyleRuleFunction>(self),
                                                   parent_sheet);
      break;
    case kScope:
      rule = MakeGarbageCollected<CSSScopeRule>(To<StyleRuleScope>(self),
                                                parent_sheet);
      break;
    case kSupports:
      rule = MakeGarbageCollected<CSSSupportsRule>(To<StyleRuleSupports>(self),
                                                   parent_sheet);
      break;
    case kImport:
      rule = MakeGarbageCollected<CSSImportRule>(To<StyleRuleImport>(self),
                                                 parent_sheet);
      break;
    case kKeyframes:
      rule = MakeGarbageCollected<CSSKeyframesRule>(
          To<StyleRuleKeyframes>(self), parent_sheet);
      break;
    case kLayerBlock:
      rule = MakeGarbageCollected<CSSLayerBlockRule>(
          To<StyleRuleLayerBlock>(self), parent_sheet);
      break;
    case kLayerStatement:
      rule = MakeGarbageCollected<CSSLayerStatementRule>(
          To<StyleRuleLayerStatement>(self), parent_sheet);
      break;
    case kNamespace:
      rule = MakeGarbageCollected<CSSNamespaceRule>(
          To<StyleRuleNamespace>(self), parent_sheet);
      break;
    case kContainer:
      rule = MakeGarbageCollected<CSSContainerRule>(
          To<StyleRuleContainer>(self), parent_sheet);
      break;
    case kCounterStyle:
      rule = MakeGarbageCollected<CSSCounterStyleRule>(
          To<StyleRuleCounterStyle>(self), parent_sheet);
      break;
    case kStartingStyle:
      rule = MakeGarbageCollected<CSSStartingStyleRule>(
          To<StyleRuleStartingStyle>(self), parent_sheet);
      break;
    case kViewTransition:
      rule = MakeGarbageCollected<CSSViewTransitionRule>(
          To<StyleRuleViewTransition>(self), parent_sheet);
      break;
    case kPositionTry:
      rule = MakeGarbageCollected<CSSPositionTryRule>(
          To<StyleRulePositionTry>(self), parent_sheet);
      break;
    case kCustomMedia:
      rule = MakeGarbageCollected<CSSCustomMediaRule>(
          To<StyleRuleCustomMedia>(self), parent_sheet);
      break;
    case kMixin:
      rule = MakeGarbageCollected<CSSMixinRule>(To<StyleRuleMixin>(self),
                                                parent_sheet);
      break;
    case kApplyMixin:
      rule = MakeGarbageCollected<CSSApplyMixinRule>(
          To<StyleRuleApplyMixin>(self), parent_sheet);
      break;
    case kContents:
      rule = MakeGarbageCollected<CSSContentsMixinRule>(
          To<StyleRuleContentsStatement>(self), parent_sheet);
      break;
    case kFontFeature:
    case kKeyframe:
    case kCharset:
      NOTREACHED();
  }
  if (parent_rule) {
    rule->SetParentRule(parent_rule);
  }
  return rule;
}

unsigned StyleRule::AverageSizeInBytes() {
  return sizeof(StyleRule) + sizeof(CSSSelector) +
         CSSPropertyValueSet::AverageSizeInBytes();
}

StyleRule::StyleRule(base::PassKey<StyleRule>,
                     base::span<CSSSelector> selector_vector,
                     CSSPropertyValueSet* properties,
                     const MixinParameterBindings* mixin_parameter_bindings)
    : StyleRuleBase(kStyle),
      properties_(properties),
      mixin_parameter_bindings_(mixin_parameter_bindings) {
  CSSSelectorList::AdoptSelectorVector(selector_vector, SelectorArray());
}

StyleRule::StyleRule(base::PassKey<StyleRule>,
                     base::span<CSSSelector> selector_vector,
                     CSSLazyPropertyParser* lazy_property_parser)
    : StyleRuleBase(kStyle), lazy_property_parser_(lazy_property_parser) {
  CSSSelectorList::AdoptSelectorVector(selector_vector, SelectorArray());
}

StyleRule::StyleRule(base::PassKey<StyleRule>,
                     base::span<CSSSelector> selector_vector)
    : StyleRuleBase(kStyle) {
  CSSSelectorList::AdoptSelectorVector(selector_vector, SelectorArray());
}

StyleRule::StyleRule(base::PassKey<StyleRule>,
                     base::span<CSSSelector> selector_vector,
                     StyleRule&& other)
    : StyleRuleBase(kStyle),
      properties_(other.properties_),
      lazy_property_parser_(other.lazy_property_parser_),
      child_rules_(std::move(other.child_rules_)) {
  CSSSelectorList::AdoptSelectorVector(selector_vector, SelectorArray());
}

const CSSPropertyValueSet& StyleRule::Properties() const {
  if (!properties_) {
    properties_ = lazy_property_parser_->ParseProperties();
    lazy_property_parser_.Clear();
  }
  return *properties_;
}

StyleRule::~StyleRule() {
  // Clean up any RareData that the selectors may be owning.
  CSSSelector* selector = SelectorArray();
  for (;;) {
    bool is_last = selector->IsLastInSelectorList();
    selector->~CSSSelector();
    if (is_last) {
      break;
    }
    UNSAFE_TODO(++selector);
  }
}

MutableCSSPropertyValueSet& StyleRule::MutableProperties() {
  // Ensure properties_ is initialized.
  if (!Properties().IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRule::WrapperInsertRule(CSSStyleSheet* parent_sheet,
                                  unsigned index,
                                  StyleRuleBase* rule) {
  EnsureChildRules();
  child_rules_->insert(index, rule);
  if (parent_sheet) {
    parent_sheet->Contents()->NotifyRuleChanged(rule);
  }
}

void StyleRule::WrapperRemoveRule(CSSStyleSheet* parent_sheet, unsigned index) {
  if (parent_sheet) {
    parent_sheet->Contents()->NotifyRuleChanged((*child_rules_)[index]);
  }
  child_rules_->erase(UNSAFE_TODO(child_rules_->begin() + index));
}

bool StyleRule::PropertiesHaveFailedOrCanceledSubresources() const {
  return properties_ && properties_->HasFailedOrCanceledSubresources();
}

bool StyleRule::HasParsedProperties() const {
  // StyleRule should only have one of {lazy_property_parser_, properties_} set.
  DCHECK(lazy_property_parser_ || properties_);
  DCHECK(!lazy_property_parser_ || !properties_);
  return !lazy_property_parser_;
}

void StyleRule::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(lazy_property_parser_);
  visitor->Trace(child_rules_);
  visitor->Trace(mixin_parameter_bindings_);

  const CSSSelector* current = SelectorArray();
  do {
    visitor->Trace(*current);
  } while (!(UNSAFE_TODO(current++))->IsLastInSelectorListForOilpan());

  StyleRuleBase::TraceAfterDispatch(visitor);
}

namespace {

HeapVector<Member<StyleRuleBase>> CloneRules(
    const HeapVector<Member<StyleRuleBase>>& old_rules,
    StyleRule* new_parent,
    const MixinParameterBindings* mixin_parameter_bindings) {
  HeapVector<Member<StyleRuleBase>> result;
  for (StyleRuleBase* old_rule : old_rules) {
    result.push_back(old_rule->Clone(new_parent, mixin_parameter_bindings));
  }
  return result;
}

template <typename T>
StyleRuleBase* CloneGroupRule(
    T* group_rule,
    StyleRule* new_parent,
    const MixinParameterBindings* mixin_parameter_bindings) {
  return MakeGarbageCollected<T>(
      *group_rule, CloneRules(group_rule->ChildRules(), new_parent,
                              mixin_parameter_bindings));
}

// Make sure that the FakeParentRuleForDeclarations, if any,
// gets our parent as parent. In particular, we'd like any
// StyleRuleNestedDeclarations in there to get our selector
// (it copies the parent selector during clone), not the
// dummy parent selector that's there from parsing and which
// may have the wrong specificity.
StyleRule* CloneFakeParentRule(
    StyleRule* old_inner_rule,
    StyleRule* new_parent,
    const MixinParameterBindings* mixin_parameter_bindings) {
  HeapVector<CSSSelector> selectors =
      CSSSelectorList::Copy(new_parent->FirstSelector());
  auto* new_rule = StyleRule::Create(
      selectors, old_inner_rule->Properties().ImmutableCopyIfNeeded(),
      mixin_parameter_bindings);
  for (StyleRuleBase* child_rule : *old_inner_rule->ChildRules()) {
    new_rule->AddChildRule(
        child_rule->Clone(new_rule, mixin_parameter_bindings));
  }
  return new_rule;
}

}  // namespace

StyleRuleBase* StyleRuleBase::Clone(
    StyleRule* new_parent,
    const MixinParameterBindings* mixin_parameter_bindings) {
  switch (GetType()) {
    case kStyle: {
      HeapVector<CSSSelector> selectors;
      CSSSelectorList::Renest(To<StyleRule>(this)->FirstSelector(), new_parent,
                              selectors);
      auto* new_rule = StyleRule::Create(
          selectors, To<StyleRule>(this)->Properties().ImmutableCopyIfNeeded(),
          mixin_parameter_bindings);
      if (GCedHeapVector<Member<StyleRuleBase>>* child_rules =
              To<StyleRule>(this)->ChildRules()) {
        for (StyleRuleBase* child_rule : *child_rules) {
          new_rule->AddChildRule(
              child_rule->Clone(new_rule, mixin_parameter_bindings));
        }
      }
      return new_rule;
    }
    case kScope: {
      const StyleScope* old_style_scope =
          &To<StyleRuleScope>(this)->GetStyleScope();
      const StyleScope* new_style_scope = old_style_scope->Clone(new_parent);
      CHECK(new_style_scope);
      HeapVector<Member<StyleRuleBase>> new_child_rules = CloneRules(
          To<StyleRuleScope>(this)->ChildRules(),
          new_style_scope->RuleForNesting(), mixin_parameter_bindings);
      return MakeGarbageCollected<StyleRuleScope>(*new_style_scope,
                                                  std::move(new_child_rules));
    }
    case kLayerBlock:
      return CloneGroupRule(To<StyleRuleLayerBlock>(this), new_parent,
                            mixin_parameter_bindings);
    case kContainer: {
      StyleRuleContainer* container_rule = To<StyleRuleContainer>(this);
      return MakeGarbageCollected<StyleRuleContainer>(
          *MakeGarbageCollected<ContainerQuery>(
              container_rule->GetContainerQuery()),
          CloneRules(container_rule->ChildRules(), new_parent,
                     mixin_parameter_bindings));
    }
    case kMedia:
      return CloneGroupRule(To<StyleRuleMedia>(this), new_parent,
                            mixin_parameter_bindings);
    case kRoute:
      return MakeGarbageCollected<StyleRuleRoute>(To<StyleRuleRoute>(*this));
    case kNavigation:
      return CloneGroupRule(To<StyleRuleNavigation>(this), new_parent,
                            mixin_parameter_bindings);
    case kSupports:
      return CloneGroupRule(To<StyleRuleSupports>(this), new_parent,
                            mixin_parameter_bindings);
    case kStartingStyle:
      return CloneGroupRule(To<StyleRuleStartingStyle>(this), new_parent,
                            mixin_parameter_bindings);
    case kPage: {
      return MakeGarbageCollected<StyleRulePage>(
          To<StyleRulePage>(this)->SelectorList()->Renest(new_parent),
          To<StyleRulePage>(this)->Properties().ImmutableCopyIfNeeded(),
          CloneRules(To<StyleRulePage>(this)->ChildRules(), new_parent,
                     mixin_parameter_bindings));
    }
    case kMixin:
      return CloneGroupRule(To<StyleRuleMixin>(this), new_parent,
                            mixin_parameter_bindings);
    case kApplyMixin: {
      auto* apply_rule = To<StyleRuleApplyMixin>(this);
      StyleRule* old_inner_rule = apply_rule->FakeParentRuleForDeclarations();
      if (!old_inner_rule || !old_inner_rule->ChildRules()) {
        return this;
      }
      return MakeGarbageCollected<StyleRuleApplyMixin>(
          apply_rule->GetName(), apply_rule->GetArguments(),
          CloneFakeParentRule(old_inner_rule, new_parent,
                              mixin_parameter_bindings));
    }
    case kContents: {
      auto* contents_rule = To<StyleRuleContentsStatement>(this);
      StyleRule* old_inner_rule = contents_rule->FakeParentRuleForFallback();
      if (!old_inner_rule || !old_inner_rule->ChildRules()) {
        return this;
      }
      return MakeGarbageCollected<StyleRuleContentsStatement>(
          CloneFakeParentRule(old_inner_rule, new_parent,
                              mixin_parameter_bindings));
    }
    case kNestedDeclarations: {
      auto* nested_declarations_rule = To<StyleRuleNestedDeclarations>(this);
      // Nested declaration rules are different from regular nested style rules,
      // since they don't refer to their parent rule with any '&' selector.
      // Instead the outer selector list is *copied* parse-time. Now that we're
      // being re-nested, we need to create a new StyleRuleNestedDeclarations
      // rule, again with a copy of the new parent rule's selector list.
      //
      // The copying behavior does not apply to nested declaration rules held
      // by @scope rules, however, since they always just behave like
      // :where(:scope).
      if (nested_declarations_rule->NestingType() == CSSNestingType::kScope) {
        return this;
      }
      StyleRule* old_inner_rule = nested_declarations_rule->InnerStyleRule();
      HeapVector<CSSSelector> selectors =
          CSSSelectorList::Copy(new_parent->FirstSelector());
      auto* new_inner_rule = StyleRule::Create(
          selectors, old_inner_rule->Properties().ImmutableCopyIfNeeded(),
          mixin_parameter_bindings);
      return MakeGarbageCollected<StyleRuleNestedDeclarations>(
          nested_declarations_rule->NestingType(), new_inner_rule);
    }
    case kFunctionDeclarations:
      return MakeGarbageCollected<StyleRuleFunctionDeclarations>(
          To<StyleRuleFunctionDeclarations>(*this));
    case kFunction: {
      StyleRuleFunction* function_rule = To<StyleRuleFunction>(this);
      HeapVector<Member<StyleRuleBase>> result = CloneRules(
          function_rule->ChildRules(), new_parent, mixin_parameter_bindings);
      return MakeGarbageCollected<StyleRuleFunction>(
          function_rule->Name(), function_rule->GetParameters(),
          std::move(result), function_rule->GetReturnType());
    }
    case kProperty:
      return MakeGarbageCollected<StyleRuleProperty>(
          To<StyleRuleProperty>(*this));
    case kPageMargin:
      return MakeGarbageCollected<StyleRulePageMargin>(
          To<StyleRulePageMargin>(*this));
    case kFontFace:
      return MakeGarbageCollected<StyleRuleFontFace>(
          To<StyleRuleFontFace>(*this));
    case kFontPaletteValues:
      return MakeGarbageCollected<StyleRuleFontPaletteValues>(
          To<StyleRuleFontPaletteValues>(*this));
    case kFontFeatureValues:
      return MakeGarbageCollected<StyleRuleFontFeatureValues>(
          To<StyleRuleFontFeatureValues>(*this));
    case kFontFeature:
      return MakeGarbageCollected<StyleRuleFontFeature>(
          To<StyleRuleFontFeature>(*this));
    case kImport:
      return MakeGarbageCollected<StyleRuleImport>(To<StyleRuleImport>(*this));
    case kKeyframes:
      return MakeGarbageCollected<StyleRuleKeyframes>(
          To<StyleRuleKeyframes>(*this));
    case kLayerStatement:
      return MakeGarbageCollected<StyleRuleLayerStatement>(
          To<StyleRuleLayerStatement>(*this));
    case kNamespace:
      return MakeGarbageCollected<StyleRuleNamespace>(
          To<StyleRuleNamespace>(*this));
    case kCounterStyle:
      return MakeGarbageCollected<StyleRuleCounterStyle>(
          To<StyleRuleCounterStyle>(*this));
    case kKeyframe:
      return MakeGarbageCollected<StyleRuleKeyframe>(
          To<StyleRuleKeyframe>(*this));
    case kCharset:
      return MakeGarbageCollected<StyleRuleCharset>(
          To<StyleRuleCharset>(*this));
    case kViewTransition:
      return MakeGarbageCollected<StyleRuleViewTransition>(
          To<StyleRuleViewTransition>(*this));
    case kPositionTry:
      return MakeGarbageCollected<StyleRulePositionTry>(
          To<StyleRulePositionTry>(*this));
    case kCustomMedia:
      return MakeGarbageCollected<StyleRuleCustomMedia>(
          To<StyleRuleCustomMedia>(*this));
  }
}

StyleRuleProperty::StyleRuleProperty(const String& name,
                                     CSSPropertyValueSet* properties)
    : StyleRuleBase(kProperty), name_(name), properties_(properties) {}

StyleRuleProperty::StyleRuleProperty(const StyleRuleProperty& property_rule)
    : StyleRuleBase(property_rule),
      name_(property_rule.name_),
      properties_(property_rule.properties_->MutableCopy()) {}

MutableCSSPropertyValueSet& StyleRuleProperty::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

const CSSValue* StyleRuleProperty::GetSyntax() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kSyntax);
}

const CSSValue* StyleRuleProperty::Inherits() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kInherits);
}

const CSSValue* StyleRuleProperty::GetInitialValue() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kInitialValue);
}

bool StyleRuleProperty::SetNameText(const ExecutionContext* execution_context,
                                    const String& name_text) {
  DCHECK(!name_text.IsNull());
  String name = CSSParser::ParseCustomPropertyName(name_text);
  if (!name)
    return false;

  name_ = name;
  return true;
}

void StyleRuleProperty::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleFontFace::StyleRuleFontFace(CSSPropertyValueSet* properties)
    : StyleRuleBase(kFontFace), properties_(properties) {}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& font_face_rule)
    : StyleRuleBase(font_face_rule),
      properties_(font_face_rule.properties_->MutableCopy()) {}

MutableCSSPropertyValueSet& StyleRuleFontFace::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleFontFace::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleScope::StyleRuleScope(const StyleScope& style_scope,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(kScope, std::move(rules)), style_scope_(&style_scope) {}

void StyleRuleScope::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(style_scope_);
  StyleRuleGroup::TraceAfterDispatch(visitor);
}

StyleRuleGroup::StyleRuleGroup(RuleType type,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleBase(type), child_rules_(std::move(rules)) {}

void StyleRuleGroup::WrapperInsertRule(CSSStyleSheet* parent_sheet,
                                       unsigned index,
                                       StyleRuleBase* rule) {
  child_rules_.insert(index, rule);
  if (parent_sheet) {
    parent_sheet->Contents()->NotifyRuleChanged(rule);
  }
}

void StyleRuleGroup::WrapperRemoveRule(CSSStyleSheet* parent_sheet,
                                       unsigned index) {
  if (parent_sheet) {
    parent_sheet->Contents()->NotifyRuleChanged(child_rules_[index]);
  }
  child_rules_.EraseAt(index);
}

void StyleRuleGroup::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(child_rules_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

// static
String StyleRuleBase::LayerNameAsString(
    const StyleRuleBase::LayerName& name_parts) {
  StringBuilder result;
  for (const auto& part : name_parts) {
    if (result.length()) {
      result.Append(".");
    }
    SerializeIdentifier(part, result);
  }
  return result.ReleaseString();
}

StyleRuleLayerBlock::StyleRuleLayerBlock(
    LayerName name,
    HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(kLayerBlock, std::move(rules)), name_(std::move(name)) {}

StyleRuleLayerBlock::StyleRuleLayerBlock(
    const StyleRuleLayerBlock& other,
    HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleLayerBlock(other.name_, std::move(rules)) {}

void StyleRuleLayerBlock::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleGroup::TraceAfterDispatch(visitor);
}

String StyleRuleLayerBlock::GetNameAsString() const {
  return LayerNameAsString(name_);
}

StyleRuleLayerStatement::StyleRuleLayerStatement(Vector<LayerName>&& names)
    : StyleRuleBase(kLayerStatement), names_(std::move(names)) {}

StyleRuleLayerStatement::StyleRuleLayerStatement(
    const StyleRuleLayerStatement& other) = default;

void StyleRuleLayerStatement::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
}

Vector<String> StyleRuleLayerStatement::GetNamesAsStrings() const {
  Vector<String> result;
  for (const auto& name : names_) {
    result.push_back(LayerNameAsString(name));
  }
  return result;
}

StyleRulePage::StyleRulePage(const CSSSelectorList* selector_list,
                             CSSPropertyValueSet* properties,
                             HeapVector<Member<StyleRuleBase>> child_rules)
    : StyleRuleGroup(kPage, std::move(child_rules)),
      properties_(properties),
      selector_list_(selector_list) {}

MutableCSSPropertyValueSet& StyleRulePage::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRulePage::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(selector_list_);
  StyleRuleGroup::TraceAfterDispatch(visitor);
}

StyleRulePageMargin::StyleRulePageMargin(CSSAtRuleID id,
                                         CSSPropertyValueSet* properties)
    : StyleRuleBase(kPageMargin), id_(id), properties_(properties) {}

StyleRulePageMargin::StyleRulePageMargin(
    const StyleRulePageMargin& page_margin_rule)
    : StyleRuleBase(page_margin_rule),
      id_(page_margin_rule.id_),
      properties_(page_margin_rule.properties_->MutableCopy()) {}

MutableCSSPropertyValueSet& StyleRulePageMargin::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRulePageMargin::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleCondition::StyleRuleCondition(RuleType type,
                                       HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(type, std::move(rules)) {}

StyleRuleCondition::StyleRuleCondition(RuleType type,
                                       const String& condition_text,
                                       HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(type, std::move(rules)), condition_text_(condition_text) {}

StyleRuleMedia::StyleRuleMedia(const MediaQuerySet* media,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleCondition(kMedia, std::move(rules)), media_queries_(media) {}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& other,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleMedia(other.media_queries_, std::move(rules)) {}

void StyleRuleMedia::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleCondition::TraceAfterDispatch(visitor);
  visitor->Trace(media_queries_);
}

StyleRuleSupports::StyleRuleSupports(const String& condition_text,
                                     bool condition_is_supported,
                                     HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleCondition(kSupports, condition_text, std::move(rules)),
      condition_is_supported_(condition_is_supported) {}

StyleRuleSupports::StyleRuleSupports(const StyleRuleSupports& other,
                                     HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleSupports(other.condition_text_,
                        other.condition_is_supported_,
                        std::move(rules)) {}

void StyleRuleSupports::SetConditionText(
    const ExecutionContext* execution_context,
    String value) {
  CSSParserTokenStream stream(value);
  auto* context = MakeGarbageCollected<CSSParserContext>(*execution_context);
  CSSParserImpl parser(context);

  CSSSupportsParser::Result result =
      CSSSupportsParser::ConsumeSupportsCondition(stream, parser);
  condition_text_ = value;
  condition_is_supported_ = result == CSSSupportsParser::Result::kSupported;
}

StyleRuleContainer::StyleRuleContainer(ContainerQuery& container_query,
                                       HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleCondition(kContainer,
                         container_query.ToString(),
                         std::move(rules)),
      container_query_(&container_query) {}

StyleRuleContainer::StyleRuleContainer(const StyleRuleContainer& other,
                                       HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleContainer(*other.container_query_, std::move(rules)) {}

void StyleRuleContainer::SetConditionText(
    const ExecutionContext* execution_context,
    StyleSheetContents* parent_sheet_contents,
    String value) {
  auto* context = MakeGarbageCollected<CSSParserContext>(*execution_context);
  ContainerQueryParser parser(*context);

  if (const ConditionalExpNode* exp_node = parser.ParseCondition(value)) {
    condition_text_ = exp_node->Serialize();

    ContainerSelector selector(container_query_->Selector().Name(), *exp_node);
    container_query_ =
        MakeGarbageCollected<ContainerQuery>(std::move(selector), exp_node);

    if (parent_sheet_contents) {
      parent_sheet_contents->NotifyRuleChanged(this);
    }
  }
}

void StyleRuleContainer::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(container_query_);
  StyleRuleCondition::TraceAfterDispatch(visitor);
}

StyleRuleNavigation::StyleRuleNavigation(
    NavigationQuery* query,
    HeapVector<Member<StyleRuleBase>> child_rules)
    : StyleRuleCondition(kNavigation, std::move(child_rules)),
      navigation_query_(query) {}

StyleRuleNavigation::StyleRuleNavigation(
    const StyleRuleNavigation& other,
    HeapVector<Member<StyleRuleBase>> child_rules)
    : StyleRuleCondition(kNavigation, std::move(child_rules)),
      navigation_query_(other.navigation_query_) {}

void StyleRuleNavigation::TraceAfterDispatch(Visitor* v) const {
  v->Trace(navigation_query_);
  StyleRuleCondition::TraceAfterDispatch(v);
}

StyleRuleStartingStyle::StyleRuleStartingStyle(
    HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(kStartingStyle, std::move(rules)) {}

void StyleRuleFunction::Parameter::Trace(blink::Visitor* visitor) const {
  visitor->Trace(default_value);
}

StyleRuleFunction::StyleRuleFunction(
    AtomicString name,
    HeapVector<StyleRuleFunction::Parameter> parameters,
    HeapVector<Member<StyleRuleBase>> child_rules,
    CSSSyntaxDefinition return_type)
    : StyleRuleGroup(kFunction, std::move(child_rules)),
      name_(std::move(name)),
      parameters_(std::move(parameters)),
      return_type_(return_type) {}

void StyleRuleFunction::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleGroup::TraceAfterDispatch(visitor);
  visitor->Trace(parameters_);
}

StyleRuleMixin::StyleRuleMixin(
    AtomicString name,
    HeapVector<StyleRuleFunction::Parameter> parameters,
    HeapVector<Member<StyleRuleBase>> child_rules)
    : StyleRuleGroup(kMixin, child_rules),
      name_(std::move(name)),
      parameters_(std::move(parameters)) {}

StyleRuleMixin::StyleRuleMixin(const StyleRuleMixin& other,
                               HeapVector<Member<StyleRuleBase>> child_rules)
    : StyleRuleGroup(kMixin, child_rules),
      name_(other.name_),
      parameters_(other.parameters_) {}

void StyleRuleMixin::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleGroup::TraceAfterDispatch(visitor);
  visitor->Trace(parameters_);
}

void StyleRuleApplyMixin::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
  visitor->Trace(fake_parent_rule_for_declarations_);
  visitor->Trace(arguments_);
}

void StyleRuleContentsStatement::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
  visitor->Trace(fake_parent_rule_for_fallback_);
}

StyleRuleCustomMedia::StyleRuleCustomMedia(AtomicString name,
                                           MediaQuerySet* media_query_set)
    : StyleRuleBase(kCustomMedia),
      name_(std::move(name)),
      value_(media_query_set) {}

StyleRuleCustomMedia::StyleRuleCustomMedia(AtomicString name, bool value)
    : StyleRuleBase(kCustomMedia), name_(std::move(name)), value_(value) {}

void StyleRuleCustomMedia::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleBase::TraceAfterDispatch(visitor);
  if (IsMediaQueryValue()) {
    visitor->Trace(std::get<Member<const MediaQuerySet>>(value_));
  }
}

unsigned MixinParameterBindings::ComputeHash() const {
  unsigned hash = parent_mixin_ ? parent_mixin_->GetHash() : 1234;
  for (const auto& [key, value] : bindings_) {
    hash = HashInts(hash, HashInts(key.Impl()->GetHash(),
                                   value.value ? value.value->Hash() : 5678));
  }
  return hash;
}

bool MixinParameterBindings::operator==(
    const MixinParameterBindings& other) const {
  if (bindings_ != other.bindings_) {
    return false;
  }
  return base::ValuesEquivalent(parent_mixin_, other.parent_mixin_);
}

}  // namespace blink
