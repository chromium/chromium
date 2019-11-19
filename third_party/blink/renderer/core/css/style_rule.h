/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;

class CORE_EXPORT StyleRuleBase : public GarbageCollected<StyleRuleBase> {
 public:
  enum RuleType {
    kCharset,
    kStyle,
    kImport,
    kMedia,
    kFontFace,
    kPage,
    kProperty,
    kKeyframes,
    kKeyframe,
    kNamespace,
    kSupports,
    kViewport,
  };

  RuleType GetType() const { return static_cast<RuleType>(type_); }

  bool IsCharsetRule() const { return GetType() == kCharset; }
  bool IsFontFaceRule() const { return GetType() == kFontFace; }
  bool IsKeyframesRule() const { return GetType() == kKeyframes; }
  bool IsKeyframeRule() const { return GetType() == kKeyframe; }
  bool IsNamespaceRule() const { return GetType() == kNamespace; }
  bool IsMediaRule() const { return GetType() == kMedia; }
  bool IsPageRule() const { return GetType() == kPage; }
  bool IsPropertyRule() const { return GetType() == kProperty; }
  bool IsStyleRule() const { return GetType() == kStyle; }
  bool IsSupportsRule() const { return GetType() == kSupports; }
  bool IsViewportRule() const { return GetType() == kViewport; }
  bool IsImportRule() const { return GetType() == kImport; }

  StyleRuleBase* Copy() const;

  // FIXME: There shouldn't be any need for the null parent version.
  CSSRule* CreateCSSOMWrapper(CSSStyleSheet* parent_sheet = nullptr) const;
  CSSRule* CreateCSSOMWrapper(CSSRule* parent_rule) const;

  void Trace(blink::Visitor*);
  void TraceAfterDispatch(blink::Visitor* visitor) {}
  void FinalizeGarbageCollectedObject();

  // ~StyleRuleBase should be public, because non-public ~StyleRuleBase
  // causes C2248 error : 'blink::StyleRuleBase::~StyleRuleBase' : cannot
  // access protected member declared in class 'blink::StyleRuleBase' when
  // compiling 'source\wtf\refcounted.h' by using msvc.
  ~StyleRuleBase() = default;

 protected:
  StyleRuleBase(RuleType type) : type_(type) {}
  StyleRuleBase(const StyleRuleBase& rule) : type_(rule.type_) {}

 private:
  CSSRule* CreateCSSOMWrapper(CSSStyleSheet* parent_sheet,
                              CSSRule* parent_rule) const;

  unsigned type_ : 5;
};

// A single rule from a stylesheet. Contains a selector list (one or more
// complex selectors) and a collection of style properties to be applied where
// those selectors match. These are output by CSSParserImpl.
class CORE_EXPORT StyleRule : public StyleRuleBase {
 public:
  // Adopts the selector list
  StyleRule(CSSSelectorList, CSSPropertyValueSet*);
  StyleRule(CSSSelectorList, CSSLazyPropertyParser*);
  StyleRule(const StyleRule&);
  ~StyleRule();

  const CSSSelectorList& SelectorList() const { return selector_list_; }
  const CSSPropertyValueSet& Properties() const;
  MutableCSSPropertyValueSet& MutableProperties();

  void WrapperAdoptSelectorList(CSSSelectorList selectors) {
    selector_list_ = std::move(selectors);
  }

  StyleRule* Copy() const { return MakeGarbageCollected<StyleRule>(*this); }

  static unsigned AverageSizeInBytes();

  // Helper methods to avoid parsing lazy properties when not needed.
  bool PropertiesHaveFailedOrCanceledSubresources() const;
  bool ShouldConsiderForMatchingRules(bool include_empty_rules) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  friend class CSSLazyParsingTest;
  bool HasParsedProperties() const;

  // Whether or not we should consider this for matching rules. Usually we try
  // to avoid considering empty property sets, as an optimization. This is
  // not possible for lazy properties, which always need to be considered. The
  // lazy parser does its best to avoid lazy parsing for properties that look
  // empty due to lack of tokens.
  enum ConsiderForMatching {
    kAlwaysConsider,
    kConsiderIfNonEmpty,
  };
  mutable ConsiderForMatching should_consider_for_matching_rules_;

  CSSSelectorList selector_list_;
  mutable Member<CSSPropertyValueSet> properties_;
  mutable Member<CSSLazyPropertyParser> lazy_property_parser_;
};

class CORE_EXPORT StyleRuleFontFace : public StyleRuleBase {
 public:
  StyleRuleFontFace(CSSPropertyValueSet*);
  StyleRuleFontFace(const StyleRuleFontFace&);
  ~StyleRuleFontFace();

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  StyleRuleFontFace* Copy() const {
    return MakeGarbageCollected<StyleRuleFontFace>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  Member<CSSPropertyValueSet> properties_;  // Cannot be null.
};

class StyleRulePage : public StyleRuleBase {
 public:
  StyleRulePage(CSSSelectorList, CSSPropertyValueSet*);
  StyleRulePage(const StyleRulePage&);
  ~StyleRulePage();

  const CSSSelector* Selector() const { return selector_list_.First(); }
  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  void WrapperAdoptSelectorList(CSSSelectorList selectors) {
    selector_list_ = std::move(selectors);
  }

  StyleRulePage* Copy() const {
    return MakeGarbageCollected<StyleRulePage>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  Member<CSSPropertyValueSet> properties_;  // Cannot be null.
  CSSSelectorList selector_list_;
};

class StyleRuleProperty : public StyleRuleBase {
 public:
  static StyleRuleProperty* Create(const String& name,
                                   CSSPropertyValueSet* properties) {
    return MakeGarbageCollected<StyleRuleProperty>(name, properties);
  }

  StyleRuleProperty(const String& name, CSSPropertyValueSet*);
  StyleRuleProperty(const StyleRuleProperty&);
  ~StyleRuleProperty();

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();
  const String& GetName() const { return name_; }

  StyleRuleProperty* Copy() const {
    return MakeGarbageCollected<StyleRuleProperty>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  String name_;
  Member<CSSPropertyValueSet> properties_;
};

class CORE_EXPORT StyleRuleGroup : public StyleRuleBase {
 public:
  const HeapVector<Member<StyleRuleBase>>& ChildRules() const {
    return child_rules_;
  }

  void WrapperInsertRule(unsigned, StyleRuleBase*);
  void WrapperRemoveRule(unsigned);

  void TraceAfterDispatch(blink::Visitor*);

 protected:
  StyleRuleGroup(RuleType, HeapVector<Member<StyleRuleBase>>& adopt_rule);
  StyleRuleGroup(const StyleRuleGroup&);

 private:
  HeapVector<Member<StyleRuleBase>> child_rules_;
};

class CORE_EXPORT StyleRuleCondition : public StyleRuleGroup {
 public:
  String ConditionText() const { return condition_text_; }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    StyleRuleGroup::TraceAfterDispatch(visitor);
  }

 protected:
  StyleRuleCondition(RuleType, HeapVector<Member<StyleRuleBase>>& adopt_rule);
  StyleRuleCondition(RuleType,
                     const String& condition_text,
                     HeapVector<Member<StyleRuleBase>>& adopt_rule);
  StyleRuleCondition(const StyleRuleCondition&);
  String condition_text_;
};

class CORE_EXPORT StyleRuleMedia : public StyleRuleCondition {
 public:
  StyleRuleMedia(scoped_refptr<MediaQuerySet>,
                 HeapVector<Member<StyleRuleBase>>& adopt_rules);
  StyleRuleMedia(const StyleRuleMedia&);

  MediaQuerySet* MediaQueries() const { return media_queries_.get(); }

  StyleRuleMedia* Copy() const {
    return MakeGarbageCollected<StyleRuleMedia>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  scoped_refptr<MediaQuerySet> media_queries_;
};

class StyleRuleSupports : public StyleRuleCondition {
 public:
  StyleRuleSupports(const String& condition_text,
                    bool condition_is_supported,
                    HeapVector<Member<StyleRuleBase>>& adopt_rules);
  StyleRuleSupports(const StyleRuleSupports&);

  bool ConditionIsSupported() const { return condition_is_supported_; }
  StyleRuleSupports* Copy() const {
    return MakeGarbageCollected<StyleRuleSupports>(*this);
  }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    StyleRuleCondition::TraceAfterDispatch(visitor);
  }

 private:
  String condition_text_;
  bool condition_is_supported_;
};

class StyleRuleViewport : public StyleRuleBase {
 public:
  explicit StyleRuleViewport(CSSPropertyValueSet*);
  explicit StyleRuleViewport(const StyleRuleViewport&);
  ~StyleRuleViewport();

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  StyleRuleViewport* Copy() const {
    return MakeGarbageCollected<StyleRuleViewport>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  Member<CSSPropertyValueSet> properties_;  // Cannot be null
};

// This should only be used within the CSS Parser
class StyleRuleCharset : public StyleRuleBase {
 public:
  StyleRuleCharset() : StyleRuleBase(kCharset) {}
  void TraceAfterDispatch(blink::Visitor* visitor) {
    StyleRuleBase::TraceAfterDispatch(visitor);
  }

 private:
};

template <>
struct DowncastTraits<StyleRule> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsStyleRule();
  }
};

template <>
struct DowncastTraits<StyleRuleFontFace> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsFontFaceRule();
  }
};

template <>
struct DowncastTraits<StyleRulePage> {
  static bool AllowFrom(const StyleRuleBase& rule) { return rule.IsPageRule(); }
};

template <>
struct DowncastTraits<StyleRuleProperty> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsPropertyRule();
  }
};

template <>
struct DowncastTraits<StyleRuleMedia> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsMediaRule();
  }
};

template <>
struct DowncastTraits<StyleRuleSupports> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsSupportsRule();
  }
};

template <>
struct DowncastTraits<StyleRuleViewport> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsViewportRule();
  }
};

template <>
struct DowncastTraits<StyleRuleCharset> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsCharsetRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_H_
