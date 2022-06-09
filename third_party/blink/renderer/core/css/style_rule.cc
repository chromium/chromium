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
#include "third_party/blink/renderer/core/css/css_scroll_timeline_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/css_try_rule.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsStyleRuleBase final
    : public GarbageCollected<SameSizeAsStyleRuleBase> {
  uint8_t field;
};

ASSERT_SIZE(StyleRuleBase, SameSizeAsStyleRuleBase);

CSSRule* StyleRuleBase::CreateCSSOMWrapper(CSSStyleSheet* parent_sheet) const {
  return CreateCSSOMWrapper(parent_sheet, nullptr);
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(CSSRule* parent_rule) const {
  return CreateCSSOMWrapper(nullptr, parent_rule);
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
    case kMedia:
      To<StyleRuleMedia>(this)->TraceAfterDispatch(visitor);
      return;
    case kScrollTimeline:
      To<StyleRuleScrollTimeline>(this)->TraceAfterDispatch(visitor);
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
    case kViewport:
      To<StyleRuleViewport>(this)->TraceAfterDispatch(visitor);
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
    case kMedia:
      To<StyleRuleMedia>(this)->~StyleRuleMedia();
      return;
    case kScrollTimeline:
      To<StyleRuleScrollTimeline>(this)->~StyleRuleScrollTimeline();
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
    case kViewport:
      To<StyleRuleViewport>(this)->~StyleRuleViewport();
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
    case kMedia:
      return To<StyleRuleMedia>(this)->Copy();
    case kScrollTimeline:
      return To<StyleRuleScrollTimeline>(this)->Copy();
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
    case kViewport:
      return To<StyleRuleViewport>(this)->Copy();
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
    case kTry:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(CSSStyleSheet* parent_sheet,
                                           CSSRule* parent_rule) const {
  CSSRule* rule = nullptr;
  StyleRuleBase* self = const_cast<StyleRuleBase*>(this);
  switch (GetType()) {
    case kStyle:
      rule =
          MakeGarbageCollected<CSSStyleRule>(To<StyleRule>(self), parent_sheet);
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
    case kMedia:
      rule = MakeGarbageCollected<CSSMediaRule>(To<StyleRuleMedia>(self),
                                                parent_sheet);
      break;
    case kScrollTimeline:
      rule = MakeGarbageCollected<CSSScrollTimelineRule>(
          To<StyleRuleScrollTimeline>(self), parent_sheet);
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
    case kTry:
    case kKeyframe:
    case kCharset:
    case kViewport:
      NOTREACHED();
      return nullptr;
  }
  if (parent_rule)
    rule->SetParentRule(parent_rule);
  return rule;
}

unsigned StyleRule::AverageSizeInBytes() {
  return sizeof(StyleRule) + sizeof(CSSSelector) +
         CSSPropertyValueSet::AverageSizeInBytes();
}

StyleRule::StyleRule(CSSSelectorList selector_list,
                     CSSPropertyValueSet* properties)
    : StyleRuleBase(kStyle),
      selector_list_(std::move(selector_list)),
      properties_(properties) {}

StyleRule::StyleRule(CSSSelectorList selector_list,
                     CSSLazyPropertyParser* lazy_property_parser)
    : StyleRuleBase(kStyle),
      selector_list_(std::move(selector_list)),
      lazy_property_parser_(lazy_property_parser) {}

const CSSPropertyValueSet& StyleRule::Properties() const {
  if (!properties_) {
    properties_ = lazy_property_parser_->ParseProperties();
    lazy_property_parser_.Clear();
  }
  return *properties_;
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o),
      selector_list_(o.selector_list_.Copy()),
      properties_(o.Properties().MutableCopy()) {}

MutableCSSPropertyValueSet& StyleRule::MutableProperties() {
  // Ensure properties_ is initialized.
  if (!Properties().IsMutable())
    properties_ = properties_->MutableCopy();
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
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRulePage::StyleRulePage(CSSSelectorList selector_list,
                             CSSPropertyValueSet* properties)
    : StyleRuleBase(kPage),
      properties_(properties),
      selector_list_(std::move(selector_list)) {}

StyleRulePage::StyleRulePage(const StyleRulePage& page_rule)
    : StyleRuleBase(page_rule),
      properties_(page_rule.properties_->MutableCopy()),
      selector_list_(page_rule.selector_list_.Copy()) {}

MutableCSSPropertyValueSet& StyleRulePage::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRulePage::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(layer_);
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
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
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
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleFontFace::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  visitor->Trace(layer_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleScrollTimeline::StyleRuleScrollTimeline(
    const String& name,
    const CSSPropertyValueSet* properties)
    : StyleRuleBase(kScrollTimeline),
      name_(name),
      source_(properties->GetPropertyCSSValue(CSSPropertyID::kSource)),
      orientation_(
          properties->GetPropertyCSSValue(CSSPropertyID::kOrientation)),
      start_(properties->GetPropertyCSSValue(CSSPropertyID::kStart)),
      end_(properties->GetPropertyCSSValue(CSSPropertyID::kEnd)) {}

void StyleRuleScrollTimeline::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(orientation_);
  visitor->Trace(start_);
  visitor->Trace(end_);
  visitor->Trace(layer_);

  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleScope::StyleRuleScope(const StyleScope& style_scope,
                               HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleGroup(kScope, adopt_rules), style_scope_(&style_scope) {}

StyleRuleScope::StyleRuleScope(const StyleRuleScope& other)
    : StyleRuleGroup(other),
      style_scope_(MakeGarbageCollected<StyleScope>(*other.style_scope_)) {}

void StyleRuleScope::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(style_scope_);
  StyleRuleGroup::TraceAfterDispatch(visitor);
}

StyleRuleGroup::StyleRuleGroup(RuleType type,
                               HeapVector<Member<StyleRuleBase>>& adopt_rule)
    : StyleRuleBase(type) {
  child_rules_.swap(adopt_rule);
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& group_rule)
    : StyleRuleBase(group_rule), child_rules_(group_rule.child_rules_.size()) {
  for (unsigned i = 0; i < child_rules_.size(); ++i)
    child_rules_[i] = group_rule.child_rules_[i]->Copy();
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
    if (result.length())
      result.Append(".");
    result.Append(part);
  }
  return result.ReleaseString();
}

StyleRuleLayerBlock::StyleRuleLayerBlock(
    LayerName&& name,
    HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleGroup(kLayerBlock, adopt_rules), name_(std::move(name)) {}

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
  for (const auto& name : names_)
    result.push_back(LayerNameAsString(name));
  return result;
}

StyleRuleCondition::StyleRuleCondition(
    RuleType type,
    HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleGroup(type, adopt_rules) {}

StyleRuleCondition::StyleRuleCondition(
    RuleType type,
    const String& condition_text,
    HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleGroup(type, adopt_rules), condition_text_(condition_text) {}

StyleRuleCondition::StyleRuleCondition(
    const StyleRuleCondition& condition_rule) = default;

StyleRuleMedia::StyleRuleMedia(const MediaQuerySet* media,
                               HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleCondition(kMedia, adopt_rules), media_queries_(media) {}

void StyleRuleMedia::TraceAfterDispatch(blink::Visitor* visitor) const {
  StyleRuleCondition::TraceAfterDispatch(visitor);
  visitor->Trace(media_queries_);
}

StyleRuleSupports::StyleRuleSupports(
    const String& condition_text,
    bool condition_is_supported,
    HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleCondition(kSupports, condition_text, adopt_rules),
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

StyleRuleContainer::StyleRuleContainer(
    ContainerQuery& container_query,
    HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleCondition(kContainer, container_query.ToString(), adopt_rules),
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

  if (const MediaQueryExpNode* exp_node = parser.ParseQuery(value)) {
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

StyleRuleViewport::StyleRuleViewport(CSSPropertyValueSet* properties)
    : StyleRuleBase(kViewport), properties_(properties) {}

StyleRuleViewport::StyleRuleViewport(const StyleRuleViewport& viewport_rule)
    : StyleRuleBase(viewport_rule),
      properties_(viewport_rule.properties_->MutableCopy()) {}

MutableCSSPropertyValueSet& StyleRuleViewport::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleViewport::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
