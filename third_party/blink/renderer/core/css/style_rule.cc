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

#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/css_font_face_rule.h"
#include "third_party/blink/renderer/core/css/css_font_feature_values_rule.h"
#include "third_party/blink/renderer/core/css/css_font_palette_values_rule.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_layer_block_rule.h"
#include "third_party/blink/renderer/core/css/css_layer_statement_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_namespace_rule.h"
#include "third_party/blink/renderer/core/css/css_page_rule.h"
#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"
#include "third_party/blink/renderer/core/css/css_property_rule.h"
#include "third_party/blink/renderer/core/css/css_scope_rule.h"
#include "third_party/blink/renderer/core/css/css_starting_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/css_try_rule.h"
#include "third_party/blink/renderer/core/css/css_view_transitions_rule.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_rule_view_transitions.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsStyleRuleBase final
    : public GarbageCollected<SameSizeAsStyleRuleBase> {
  uint8_t field;
};

ASSERT_SIZE(StyleRuleBase, SameSizeAsStyleRuleBase);

CSSRule* StyleRuleBase::CreateCSSOMWrapper(wtf_size_t position_hint,
                                           CSSStyleSheet* parent_sheet) const {
  return CreateCSSOMWrapper(position_hint, parent_sheet, nullptr);
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(wtf_size_t position_hint,
                                           CSSRule* parent_rule) const {
  return CreateCSSOMWrapper(position_hint, nullptr, parent_rule);
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
    case kProperty:
      To<StyleRuleProperty>(this)->TraceAfterDispatch(visitor);
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
    case kPositionFallback:
      To<StyleRulePositionFallback>(this)->TraceAfterDispatch(visitor);
      return;
    case kTry:
      To<StyleRuleTry>(this)->TraceAfterDispatch(visitor);
      return;
    case kStartingStyle:
      To<StyleRuleStartingStyle>(this)->TraceAfterDispatch(visitor);
      return;
    case kViewTransitions:
      To<StyleRuleViewTransitions>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED();
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
    case kProperty:
      To<StyleRuleProperty>(this)->~StyleRuleProperty();
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
    case kPositionFallback:
      To<StyleRulePositionFallback>(this)->~StyleRulePositionFallback();
      return;
    case kTry:
      To<StyleRuleTry>(this)->~StyleRuleTry();
      return;
    case kStartingStyle:
      To<StyleRuleStartingStyle>(this)->~StyleRuleStartingStyle();
      return;
    case kViewTransitions:
      To<StyleRuleViewTransitions>(this)->~StyleRuleViewTransitions();
      return;
  }
  NOTREACHED();
}

StyleRuleBase* StyleRuleBase::Copy() const {
  switch (GetType()) {
    case kStyle:
      return To<StyleRule>(this)->Copy();
    case kPage:
      return To<StyleRulePage>(this)->Copy();
    case kProperty:
      return To<StyleRuleProperty>(this)->Copy();
    case kFontFace:
      return To<StyleRuleFontFace>(this)->Copy();
    case kFontPaletteValues:
      return To<StyleRuleFontPaletteValues>(this)->Copy();
    case kFontFeatureValues:
      return To<StyleRuleFontFeatureValues>(this)->Copy();
    case kFontFeature:
      return To<StyleRuleFontFeature>(this)->Copy();
    case kMedia:
      return To<StyleRuleMedia>(this)->Copy();
    case kScope:
      return To<StyleRuleScope>(this)->Copy();
    case kSupports:
      return To<StyleRuleSupports>(this)->Copy();
    case kImport:
      // FIXME: Copy import rules.
      NOTREACHED();
      return nullptr;
    case kKeyframes:
      return To<StyleRuleKeyframes>(this)->Copy();
    case kLayerBlock:
      return To<StyleRuleLayerBlock>(this)->Copy();
    case kLayerStatement:
      return To<StyleRuleLayerStatement>(this)->Copy();
    case kNamespace:
      return To<StyleRuleNamespace>(this)->Copy();
    case kCharset:
    case kKeyframe:
      NOTREACHED();
      return nullptr;
    case kContainer:
      return To<StyleRuleContainer>(this)->Copy();
    case kCounterStyle:
      return To<StyleRuleCounterStyle>(this)->Copy();
    case kPositionFallback:
      return To<StyleRulePositionFallback>(this)->Copy();
    case kStartingStyle:
      return To<StyleRuleStartingStyle>(this)->Copy();
    case kViewTransitions:
      return To<StyleRuleViewTransitions>(this)->Copy();
    case kTry:
      return To<StyleRuleTry>(this)->Copy();
  }
  NOTREACHED();
  return nullptr;
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(wtf_size_t position_hint,
                                           CSSStyleSheet* parent_sheet,
                                           CSSRule* parent_rule) const {
  CSSRule* rule = nullptr;
  StyleRuleBase* self = const_cast<StyleRuleBase*>(this);
  switch (GetType()) {
    case kStyle:
      rule = MakeGarbageCollected<CSSStyleRule>(To<StyleRule>(self),
                                                parent_sheet, position_hint);
      break;
    case kPage:
      rule = MakeGarbageCollected<CSSPageRule>(To<StyleRulePage>(self),
                                               parent_sheet);
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
    case kPositionFallback:
      rule = MakeGarbageCollected<CSSPositionFallbackRule>(
          To<StyleRulePositionFallback>(self), parent_sheet);
      break;
    case kStartingStyle:
      rule = MakeGarbageCollected<CSSStartingStyleRule>(
          To<StyleRuleStartingStyle>(self), parent_sheet);
      break;
    case kViewTransitions:
      rule = MakeGarbageCollected<CSSViewTransitionsRule>(
          To<StyleRuleViewTransitions>(self), parent_sheet);
      break;
    case kTry:
      // @try rules must be child rules of @position-fallback.
      CHECK(!parent_sheet);
      rule = MakeGarbageCollected<CSSTryRule>(To<StyleRuleTry>(self));
      break;
    case kFontFeature:
    case kKeyframe:
    case kCharset:
      NOTREACHED();
      return nullptr;
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
                     CSSPropertyValueSet* properties)
    : StyleRuleBase(kStyle), properties_(properties) {
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

StyleRule::StyleRule(const StyleRule& other, size_t flattened_size)
    : StyleRuleBase(kStyle), properties_(other.Properties().MutableCopy()) {
  for (unsigned i = 0; i < flattened_size; ++i) {
    new (&SelectorArray()[i]) CSSSelector(other.SelectorArray()[i]);
  }
  if (other.child_rules_ != nullptr) {
    // Since we are getting copied, we also need to copy any child rules
    // so that both old and new can be freely mutated. This also
    // parses them eagerly (see comment in StyleSheetContents'
    // copy constructor).
    child_rules_ = MakeGarbageCollected<HeapVector<Member<StyleRuleBase>>>();
    child_rules_->ReserveInitialCapacity(other.child_rules_->size());
    for (const StyleRuleBase* child_rule : *other.child_rules_) {
      child_rules_->push_back(child_rule->Copy());
    }
  }
}

StyleRule::~StyleRule() {
  // Clean up any RareData that the selectors may be owning.
  CSSSelector* selector = SelectorArray();
  for (;;) {
    bool is_last = selector->IsLastInSelectorList();
    selector->~CSSSelector();
    if (is_last) {
      break;
    } else {
      ++selector;
    }
  }
}

MutableCSSPropertyValueSet& StyleRule::MutableProperties() {
  // Ensure properties_ is initialized.
  if (!Properties().IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
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

  const CSSSelector* current = SelectorArray();
  do {
    visitor->Trace(*current);
  } while (!(current++)->IsLastInSelectorList());

  StyleRuleBase::TraceAfterDispatch(visitor);
}

void StyleRuleBase::Reparent(StyleRule* old_parent, StyleRule* new_parent) {
  switch (GetType()) {
    case kStyle:
      CSSSelectorList::Reparent(To<StyleRule>(this)->SelectorArray(),
                                old_parent, new_parent);
      break;
    case kScope:
    case kLayerBlock:
    case kContainer:
    case kMedia:
    case kSupports:
    case kStartingStyle:
      for (StyleRuleBase* child :
           DynamicTo<StyleRuleGroup>(this)->ChildRules()) {
        child->Reparent(old_parent, new_parent);
      }
      break;
    case kPage:
    case kProperty:
    case kFontFace:
    case kFontPaletteValues:
    case kFontFeatureValues:
    case kFontFeature:
    case kImport:
    case kKeyframes:
    case kLayerStatement:
    case kNamespace:
    case kCounterStyle:
    case kPositionFallback:
    case kTry:
    case kKeyframe:
    case kCharset:
    case kViewTransitions:
      // Cannot have any child rules.
      break;
  }
}

StyleRulePage::StyleRulePage(CSSSelectorList* selector_list,
                             CSSPropertyValueSet* properties)
    : StyleRuleBase(kPage),
      properties_(properties),
      selector_list_(selector_list) {}

StyleRulePage::StyleRulePage(const StyleRulePage& page_rule)
    : StyleRuleBase(page_rule),
      properties_(page_rule.properties_->MutableCopy()),
      selector_list_(page_rule.selector_list_->Copy()) {}

MutableCSSPropertyValueSet& StyleRulePage::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRulePage::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(layer_);
  visitor->Trace(selector_list_);
  StyleRuleBase::TraceAfterDispatch(visitor);
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

void StyleRuleProperty::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(layer_);
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
  visitor->Trace(layer_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleScope::StyleRuleScope(const StyleScope& style_scope,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(kScope, std::move(rules)), style_scope_(&style_scope) {}

StyleRuleScope::StyleRuleScope(const StyleRuleScope& other)
    : StyleRuleGroup(other),
      style_scope_(MakeGarbageCollected<StyleScope>(*other.style_scope_)) {}

void StyleRuleScope::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(style_scope_);
  StyleRuleGroup::TraceAfterDispatch(visitor);
}

void StyleRuleScope::SetPreludeText(const ExecutionContext* execution_context,
                                    String value,
                                    CSSNestingType nesting_type,
                                    StyleRule* parent_rule_for_nesting,
                                    StyleSheetContents* style_sheet) {
  auto* parser_context =
      MakeGarbageCollected<CSSParserContext>(*execution_context);
  Vector<CSSParserToken, 32> tokens = CSSTokenizer(value).TokenizeToEOF();

  StyleRule* old_parent = style_scope_->RuleForNesting();
  style_scope_ = StyleScope::Parse(tokens, parser_context, nesting_type,
                                   parent_rule_for_nesting, style_sheet);

  // Reparent rules within the @scope's body.
  Reparent(old_parent, style_scope_->RuleForNesting());
}

StyleRuleGroup::StyleRuleGroup(RuleType type,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleBase(type), child_rules_(std::move(rules)) {}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& group_rule)
    : StyleRuleBase(group_rule), child_rules_(group_rule.child_rules_.size()) {
  for (unsigned i = 0; i < child_rules_.size(); ++i) {
    child_rules_[i] = group_rule.child_rules_[i]->Copy();
  }
}

void StyleRuleGroup::WrapperInsertRule(unsigned index, StyleRuleBase* rule) {
  child_rules_.insert(index, rule);
}

void StyleRuleGroup::WrapperRemoveRule(unsigned index) {
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
    result.Append(part);
  }
  return result.ReleaseString();
}

StyleRuleLayerBlock::StyleRuleLayerBlock(
    LayerName&& name,
    HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(kLayerBlock, std::move(rules)), name_(std::move(name)) {}

StyleRuleLayerBlock::StyleRuleLayerBlock(const StyleRuleLayerBlock& other) =
    default;

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

StyleRuleCondition::StyleRuleCondition(RuleType type,
                                       HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(type, std::move(rules)) {}

StyleRuleCondition::StyleRuleCondition(RuleType type,
                                       const String& condition_text,
                                       HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(type, std::move(rules)), condition_text_(condition_text) {}

StyleRuleCondition::StyleRuleCondition(
    const StyleRuleCondition& condition_rule) = default;

StyleRuleMedia::StyleRuleMedia(const MediaQuerySet* media,
                               HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleCondition(kMedia, std::move(rules)), media_queries_(media) {}

void StyleRuleMedia::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleCondition::TraceAfterDispatch(visitor);
  visitor->Trace(media_queries_);
}

StyleRuleSupports::StyleRuleSupports(const String& condition_text,
                                     bool condition_is_supported,
                                     HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleCondition(kSupports, condition_text, std::move(rules)),
      condition_is_supported_(condition_is_supported) {}

StyleRuleSupports::StyleRuleSupports(const StyleRuleSupports& supports_rule)
    : StyleRuleCondition(supports_rule),
      condition_is_supported_(supports_rule.condition_is_supported_) {}

void StyleRuleSupports::SetConditionText(
    const ExecutionContext* execution_context,
    String value) {
  CSSTokenizer tokenizer(value);
  CSSParserTokenStream stream(tokenizer);
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

StyleRuleContainer::StyleRuleContainer(const StyleRuleContainer& container_rule)
    : StyleRuleCondition(container_rule) {
  DCHECK(container_rule.container_query_);
  container_query_ =
      MakeGarbageCollected<ContainerQuery>(*container_rule.container_query_);
}

void StyleRuleContainer::SetConditionText(
    const ExecutionContext* execution_context,
    String value) {
  auto* context = MakeGarbageCollected<CSSParserContext>(*execution_context);
  ContainerQueryParser parser(*context);

  if (const MediaQueryExpNode* exp_node = parser.ParseCondition(value)) {
    condition_text_ = exp_node->Serialize();

    ContainerSelector selector(container_query_->Selector().Name(), *exp_node);
    container_query_ =
        MakeGarbageCollected<ContainerQuery>(std::move(selector), exp_node);
  }
}

void StyleRuleContainer::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(container_query_);
  StyleRuleCondition::TraceAfterDispatch(visitor);
}

StyleRuleStartingStyle::StyleRuleStartingStyle(
    HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleCondition(kStartingStyle, "", std::move(rules)) {}

}  // namespace blink
