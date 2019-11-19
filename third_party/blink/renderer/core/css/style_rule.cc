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

#include "third_party/blink/renderer/core/css/css_font_face_rule.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_namespace_rule.h"
#include "third_party/blink/renderer/core/css/css_page_rule.h"
#include "third_party/blink/renderer/core/css/css_property_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_viewport_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"

namespace blink {

struct SameSizeAsStyleRuleBase final
    : public GarbageCollected<SameSizeAsStyleRuleBase> {
  unsigned bitfields;
};

static_assert(sizeof(StyleRuleBase) <= sizeof(SameSizeAsStyleRuleBase),
              "StyleRuleBase should stay small");

CSSRule* StyleRuleBase::CreateCSSOMWrapper(CSSStyleSheet* parent_sheet) const {
  return CreateCSSOMWrapper(parent_sheet, nullptr);
}

CSSRule* StyleRuleBase::CreateCSSOMWrapper(CSSRule* parent_rule) const {
  return CreateCSSOMWrapper(nullptr, parent_rule);
}

void StyleRuleBase::Trace(blink::Visitor* visitor) {
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
    case kMedia:
      To<StyleRuleMedia>(this)->TraceAfterDispatch(visitor);
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
    case kNamespace:
      To<StyleRuleNamespace>(this)->TraceAfterDispatch(visitor);
      return;
    case kViewport:
      To<StyleRuleViewport>(this)->TraceAfterDispatch(visitor);
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
    case kMedia:
      To<StyleRuleMedia>(this)->~StyleRuleMedia();
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
    case kNamespace:
      To<StyleRuleNamespace>(this)->~StyleRuleNamespace();
      return;
    case kViewport:
      To<StyleRuleViewport>(this)->~StyleRuleViewport();
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
    case kMedia:
      return To<StyleRuleMedia>(this)->Copy();
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
    case kNamespace:
      return To<StyleRuleNamespace>(this)->Copy();
    case kCharset:
    case kKeyframe:
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
    case kMedia:
      rule = MakeGarbageCollected<CSSMediaRule>(To<StyleRuleMedia>(self),
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
    case kNamespace:
      rule = MakeGarbageCollected<CSSNamespaceRule>(
          To<StyleRuleNamespace>(self), parent_sheet);
      break;
    case kViewport:
      rule = MakeGarbageCollected<CSSViewportRule>(To<StyleRuleViewport>(self),
                                                   parent_sheet);
      break;
    case kKeyframe:
    case kCharset:
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
      should_consider_for_matching_rules_(kConsiderIfNonEmpty),
      selector_list_(std::move(selector_list)),
      properties_(properties) {}

StyleRule::StyleRule(CSSSelectorList selector_list,
                     CSSLazyPropertyParser* lazy_property_parser)
    : StyleRuleBase(kStyle),
      should_consider_for_matching_rules_(kAlwaysConsider),
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
      should_consider_for_matching_rules_(kConsiderIfNonEmpty),
      selector_list_(o.selector_list_.Copy()),
      properties_(o.Properties().MutableCopy()) {}

StyleRule::~StyleRule() = default;

MutableCSSPropertyValueSet& StyleRule::MutableProperties() {
  // Ensure properties_ is initialized.
  if (!Properties().IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

bool StyleRule::PropertiesHaveFailedOrCanceledSubresources() const {
  return properties_ && properties_->HasFailedOrCanceledSubresources();
}

bool StyleRule::ShouldConsiderForMatchingRules(bool include_empty_rules) const {
  DCHECK(should_consider_for_matching_rules_ == kAlwaysConsider || properties_);
  return include_empty_rules ||
         should_consider_for_matching_rules_ == kAlwaysConsider ||
         !properties_->IsEmpty();
}

bool StyleRule::HasParsedProperties() const {
  // StyleRule should only have one of {lazy_property_parser_, properties_} set.
  DCHECK(lazy_property_parser_ || properties_);
  DCHECK(!lazy_property_parser_ || !properties_);
  return !lazy_property_parser_;
}

void StyleRule::TraceAfterDispatch(blink::Visitor* visitor) {
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

StyleRulePage::~StyleRulePage() = default;

MutableCSSPropertyValueSet& StyleRulePage::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRulePage::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleProperty::StyleRuleProperty(const String& name,
                                     CSSPropertyValueSet* properties)
    : StyleRuleBase(kProperty), name_(name), properties_(properties) {}

StyleRuleProperty::StyleRuleProperty(const StyleRuleProperty& property_rule)
    : StyleRuleBase(property_rule),
      properties_(property_rule.properties_->MutableCopy()) {}

StyleRuleProperty::~StyleRuleProperty() = default;

MutableCSSPropertyValueSet& StyleRuleProperty::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleProperty::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

StyleRuleFontFace::StyleRuleFontFace(CSSPropertyValueSet* properties)
    : StyleRuleBase(kFontFace), properties_(properties) {}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& font_face_rule)
    : StyleRuleBase(font_face_rule),
      properties_(font_face_rule.properties_->MutableCopy()) {}

StyleRuleFontFace::~StyleRuleFontFace() = default;

MutableCSSPropertyValueSet& StyleRuleFontFace::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleFontFace::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
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

void StyleRuleGroup::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(child_rules_);
  StyleRuleBase::TraceAfterDispatch(visitor);
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

StyleRuleMedia::StyleRuleMedia(scoped_refptr<MediaQuerySet> media,
                               HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleCondition(kMedia, adopt_rules), media_queries_(media) {}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& media_rule)
    : StyleRuleCondition(media_rule) {
  if (media_rule.media_queries_)
    media_queries_ = media_rule.media_queries_->Copy();
}

StyleRuleSupports::StyleRuleSupports(
    const String& condition_text,
    bool condition_is_supported,
    HeapVector<Member<StyleRuleBase>>& adopt_rules)
    : StyleRuleCondition(kSupports, condition_text, adopt_rules),
      condition_is_supported_(condition_is_supported) {}

void StyleRuleMedia::TraceAfterDispatch(blink::Visitor* visitor) {
  StyleRuleCondition::TraceAfterDispatch(visitor);
}

StyleRuleSupports::StyleRuleSupports(const StyleRuleSupports& supports_rule)
    : StyleRuleCondition(supports_rule),
      condition_is_supported_(supports_rule.condition_is_supported_) {}

StyleRuleViewport::StyleRuleViewport(CSSPropertyValueSet* properties)
    : StyleRuleBase(kViewport), properties_(properties) {}

StyleRuleViewport::StyleRuleViewport(const StyleRuleViewport& viewport_rule)
    : StyleRuleBase(viewport_rule),
      properties_(viewport_rule.properties_->MutableCopy()) {}

StyleRuleViewport::~StyleRuleViewport() = default;

MutableCSSPropertyValueSet& StyleRuleViewport::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

void StyleRuleViewport::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
