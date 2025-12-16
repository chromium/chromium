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

#include <limits>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/mixin_parameter_bindings.h"
#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/route_matching/navigation_preposition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;
class MixinParameterBindings;
class ExecutionContext;
class NavigationQuery;

class CORE_EXPORT StyleRuleBase : public GarbageCollected<StyleRuleBase> {
 public:
  enum RuleType {
    kCharset,
    kStyle,
    kImport,
    kMedia,
    kFontFace,
    kFontPaletteValues,
    kFontFeatureValues,
    kFontFeature,
    kPage,
    kPageMargin,
    kProperty,
    kNavigation,
    kKeyframes,
    kKeyframe,
    kLayerBlock,
    kLayerStatement,
    kNestedDeclarations,
    kFunctionDeclarations,
    kNamespace,
    kContainer,
    kCounterStyle,
    kScope,
    kSupports,
    kStartingStyle,
    kViewTransition,
    kFunction,
    kMixin,
    kApplyMixin,
    kContents,
    kPositionTry,
    kCustomMedia,
    kRoute,
  };

  // Name of a cascade layer as given by an @layer rule, split at '.' into a
  // vector. Note that this may not be the full layer name if the rule is nested
  // in another @layer rule or in a layered @import.
  using LayerName = Vector<AtomicString, 1>;
  static String LayerNameAsString(const LayerName&);

  RuleType GetType() const { return static_cast<RuleType>(type_); }

  bool IsCharsetRule() const { return GetType() == kCharset; }
  bool IsContainerRule() const { return GetType() == kContainer; }
  bool IsCounterStyleRule() const { return GetType() == kCounterStyle; }
  bool IsFontFaceRule() const { return GetType() == kFontFace; }
  bool IsFontPaletteValuesRule() const {
    return GetType() == kFontPaletteValues;
  }
  bool IsFontFeatureValuesRule() const {
    return GetType() == kFontFeatureValues;
  }
  bool IsFontFeatureRule() const { return GetType() == kFontFeature; }
  bool IsKeyframesRule() const { return GetType() == kKeyframes; }
  bool IsKeyframeRule() const { return GetType() == kKeyframe; }
  bool IsLayerBlockRule() const { return GetType() == kLayerBlock; }
  bool IsLayerStatementRule() const { return GetType() == kLayerStatement; }
  bool IsFunctionDeclarationsRule() const {
    return GetType() == kFunctionDeclarations;
  }
  bool IsNestedDeclarationsRule() const {
    return GetType() == kNestedDeclarations;
  }
  bool IsNamespaceRule() const { return GetType() == kNamespace; }
  bool IsMediaRule() const { return GetType() == kMedia; }
  bool IsPageRule() const { return GetType() == kPage; }
  bool IsPageRuleMargin() const { return GetType() == kPageMargin; }
  bool IsPropertyRule() const { return GetType() == kProperty; }
  bool IsRouteRule() const { return GetType() == kRoute; }
  bool IsNavigationRule() const { return GetType() == kNavigation; }
  bool IsStyleRule() const { return GetType() == kStyle; }
  bool IsScopeRule() const { return GetType() == kScope; }
  bool IsSupportsRule() const { return GetType() == kSupports; }
  bool IsImportRule() const { return GetType() == kImport; }
  bool IsStartingStyleRule() const { return GetType() == kStartingStyle; }
  bool IsViewTransitionRule() const { return GetType() == kViewTransition; }
  bool IsConditionRule() const {
    return GetType() == kContainer || GetType() == kMedia ||
           GetType() == kSupports || GetType() == kStartingStyle;
  }
  bool IsFunctionRule() const { return GetType() == kFunction; }
  bool IsMixinRule() const { return GetType() == kMixin; }
  bool IsApplyMixinRule() const { return GetType() == kApplyMixin; }
  bool IsContentsRule() const { return GetType() == kContents; }
  bool IsPositionTryRule() const { return GetType() == kPositionTry; }
  bool IsCustomMediaRule() const { return GetType() == kCustomMedia; }

  // FIXME: There shouldn't be any need for the null parent version.
  CSSRule* CreateCSSOMWrapper(
      wtf_size_t position_hint = std::numeric_limits<wtf_size_t>::max(),
      CSSStyleSheet* parent_sheet = nullptr,
      bool trigger_use_counters = false) const;
  CSSRule* CreateCSSOMWrapper(wtf_size_t position_hint,
                              CSSRule* parent_rule,
                              bool trigger_use_counters = false) const;

  // Makes a new deep copy of the StyleRule under the new parent
  // and with the given custom @env bindings. For new_parent,
  // see CSSSelector::Renest().
  StyleRuleBase* Clone(StyleRule* new_parent,
                       const MixinParameterBindings* mixin_parameter_bindings);

  void Trace(Visitor*) const;
  void TraceAfterDispatch(blink::Visitor* visitor) const {}
  void FinalizeGarbageCollectedObject();

 protected:
  explicit StyleRuleBase(RuleType type) : type_(type) {}
  StyleRuleBase(const StyleRuleBase& rule) = default;

 private:
  CSSRule* CreateCSSOMWrapper(wtf_size_t position_hint,
                              CSSStyleSheet* parent_sheet,
                              CSSRule* parent_rule,
                              bool trigger_use_counters) const;

  const uint8_t type_;
};

// A single rule from a stylesheet. Contains a selector list (one or more
// complex selectors) and a collection of style properties to be applied where
// those selectors match. These are output by CSSParserImpl.
//
// Note that since this we generate so many StyleRule objects, and all of them
// have at least one selector, the selector list is not allocated separately as
// on a CSSSelectorList. Instead, we put the CSSSelectors immediately after the
// StyleRule object. This both saves memory (since we don't need the pointer,
// or any of the extra allocation overhead), and makes it likely that the
// CSSSelectors are on the same cache line as the StyleRule. (On the flip side,
// it makes it unlikely that the CSSSelector's RareData is on the same cache
// line as the CSSSelector itself, but it is still overall a good tradeoff
// for us.) StyleRule provides an API that is a subset of CSSSelectorList,
// partially implemented using its static member functions.
class CORE_EXPORT StyleRule : public StyleRuleBase {
  static AdditionalBytes AdditionalBytesForSelectors(size_t flattened_size) {
    constexpr size_t padding_bytes =
        base::bits::AlignUp(sizeof(StyleRule), alignof(CSSSelector)) -
        sizeof(StyleRule);
    return AdditionalBytes{(sizeof(CSSSelector) * flattened_size) +
                           padding_bytes};
  }

 public:
  // Use these to allocate the right amount of memory for the StyleRule.
  static StyleRule* Create(
      base::span<CSSSelector> selectors,
      CSSPropertyValueSet* properties,
      const MixinParameterBindings* mixin_parameter_bindings = nullptr) {
    return MakeGarbageCollected<StyleRule>(
        AdditionalBytesForSelectors(selectors.size()),
        base::PassKey<StyleRule>(), selectors, properties,
        mixin_parameter_bindings);
  }
  static StyleRule* Create(base::span<CSSSelector> selectors,
                           CSSLazyPropertyParser* lazy_property_parser) {
    return MakeGarbageCollected<StyleRule>(
        AdditionalBytesForSelectors(selectors.size()),
        base::PassKey<StyleRule>(), selectors, lazy_property_parser);
  }

  // See comment on the corresponding constructor.
  static StyleRule* Create(base::span<CSSSelector> selectors) {
    return MakeGarbageCollected<StyleRule>(
        AdditionalBytesForSelectors(selectors.size()),
        base::PassKey<StyleRule>(), selectors);
  }

  // Creates a StyleRule with the selectors changed (used by setSelectorText()).
  static StyleRule* Create(base::span<CSSSelector> selectors,
                           StyleRule&& other) {
    return MakeGarbageCollected<StyleRule>(
        AdditionalBytesForSelectors(selectors.size()),
        base::PassKey<StyleRule>(), selectors, std::move(other));
  }

  // Constructors. Note that these expect that the StyleRule has been
  // allocated on the Oilpan heap, with <flattened_size> * sizeof(CSSSelector)
  // additional bytes after the StyleRule (flattened_size is the number of
  // selectors). Do not call them directly; they are public only so that
  // MakeGarbageCollected() can call them. Instead, use Create() above or
  // Clone() below, as appropriate.
  StyleRule(base::PassKey<StyleRule>,
            base::span<CSSSelector> selector_vector,
            CSSPropertyValueSet*,
            const MixinParameterBindings*);
  StyleRule(base::PassKey<StyleRule>,
            base::span<CSSSelector> selector_vector,
            CSSLazyPropertyParser*);
  // If you use this constructor, the object will not be fully constructed until
  // you call SetProperties().
  StyleRule(base::PassKey<StyleRule>, base::span<CSSSelector> selector_vector);
  StyleRule(base::PassKey<StyleRule>,
            base::span<CSSSelector> selector_vector,
            StyleRule&&);
  StyleRule(const StyleRule&) = delete;
  ~StyleRule();

  void SetProperties(CSSPropertyValueSet* properties) {
    DCHECK_EQ(properties_.Get(), nullptr);
    properties_ = properties;
  }

  // Partial subset of the CSSSelector API.
  const CSSSelector* FirstSelector() const { return SelectorArray(); }
  const CSSSelector& SelectorAt(wtf_size_t index) const {
    return UNSAFE_TODO(SelectorArray()[index]);
  }
  CSSSelector& MutableSelectorAt(wtf_size_t index) {
    return UNSAFE_TODO(SelectorArray()[index]);
  }
  wtf_size_t SelectorIndex(const CSSSelector& selector) const {
    return static_cast<wtf_size_t>(&selector - FirstSelector());
  }
  wtf_size_t IndexOfNextSelectorAfter(wtf_size_t index) const {
    const CSSSelector& current = SelectorAt(index);
    const CSSSelector* next = CSSSelectorList::Next(current);
    if (!next) {
      return kNotFound;
    }
    return SelectorIndex(*next);
  }
  String SelectorsText() const {
    return CSSSelectorList::SelectorsText(FirstSelector());
  }

  const CSSPropertyValueSet& Properties() const;
  MutableCSSPropertyValueSet& MutableProperties();

  static unsigned AverageSizeInBytes();

  // Helper function to avoid parsing lazy properties when not needed.
  bool PropertiesHaveFailedOrCanceledSubresources() const;

  void TraceAfterDispatch(blink::Visitor*) const;

  const GCedHeapVector<Member<StyleRuleBase>>* ChildRules() const {
    return child_rules_.Get();
  }
  GCedHeapVector<Member<StyleRuleBase>>* ChildRules() {
    return child_rules_.Get();
  }
  const MixinParameterBindings* GetMixinParameterBindings() const {
    return mixin_parameter_bindings_;
  }
  void EnsureChildRules() {
    // Allocate the child rule vector only when we need it,
    // since most rules won't have children (almost by definition).
    if (child_rules_ == nullptr) {
      child_rules_ =
          MakeGarbageCollected<GCedHeapVector<Member<StyleRuleBase>>>();
    }
  }
  void AddChildRule(StyleRuleBase* child) {
    EnsureChildRules();
    child_rules_->push_back(child);
  }
  void WrapperInsertRule(CSSStyleSheet* parent_sheet,
                         unsigned index,
                         StyleRuleBase* rule);
  void WrapperRemoveRule(CSSStyleSheet* parent_sheet, unsigned index);

 private:
  friend class StyleRuleBase;
  friend class CSSLazyParsingTest;
  bool HasParsedProperties() const;

  CSSSelector* SelectorArray() {
    return reinterpret_cast<CSSSelector*>(
        base::bits::AlignUp(UNSAFE_TODO(reinterpret_cast<uint8_t*>(this + 1)),
                            alignof(CSSSelector)));
  }
  const CSSSelector* SelectorArray() const {
    return const_cast<StyleRule*>(this)->SelectorArray();
  }

  mutable Member<CSSPropertyValueSet> properties_;
  mutable Member<CSSLazyPropertyParser> lazy_property_parser_;
  Member<GCedHeapVector<Member<StyleRuleBase>>> child_rules_;
  Member<const MixinParameterBindings> mixin_parameter_bindings_;
};

class CORE_EXPORT StyleRuleFontFace : public StyleRuleBase {
 public:
  explicit StyleRuleFontFace(CSSPropertyValueSet*);
  StyleRuleFontFace(const StyleRuleFontFace&);

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<CSSPropertyValueSet> properties_;  // Cannot be null.
};

class CORE_EXPORT StyleRuleProperty : public StyleRuleBase {
 public:
  StyleRuleProperty(const String& name, CSSPropertyValueSet*);
  StyleRuleProperty(const StyleRuleProperty&);

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();
  const String& GetName() const { return name_; }
  const CSSValue* GetSyntax() const;
  const CSSValue* Inherits() const;
  const CSSValue* GetInitialValue() const;

  bool SetNameText(const ExecutionContext* execution_context,
                   const String& name_text);

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  String name_;
  Member<CSSPropertyValueSet> properties_;
};

class CORE_EXPORT StyleRuleGroup : public StyleRuleBase {
 public:
  const HeapVector<Member<StyleRuleBase>>& ChildRules() const {
    return child_rules_;
  }
  HeapVector<Member<StyleRuleBase>>& ChildRules() { return child_rules_; }

  void WrapperInsertRule(CSSStyleSheet*, unsigned, StyleRuleBase*);
  void WrapperRemoveRule(CSSStyleSheet*, unsigned);

  void TraceAfterDispatch(blink::Visitor*) const;

 protected:
  StyleRuleGroup(RuleType, HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleGroup(const StyleRuleGroup&) = delete;

 private:
  HeapVector<Member<StyleRuleBase>> child_rules_;
};

class CORE_EXPORT StyleRuleScope : public StyleRuleGroup {
 public:
  StyleRuleScope(const StyleScope&, HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleScope(const StyleRuleScope&) = delete;

  void TraceAfterDispatch(blink::Visitor*) const;

  const StyleScope& GetStyleScope() const { return *style_scope_; }

 private:
  Member<const StyleScope> style_scope_;
};

// https://www.w3.org/TR/css-cascade-5/#layer-block
class CORE_EXPORT StyleRuleLayerBlock : public StyleRuleGroup {
 public:
  StyleRuleLayerBlock(LayerName name, HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleLayerBlock(const StyleRuleLayerBlock&) = delete;
  StyleRuleLayerBlock(const StyleRuleLayerBlock&,
                      HeapVector<Member<StyleRuleBase>> rules);

  const LayerName& GetName() const { return name_; }
  String GetNameAsString() const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  LayerName name_;
};

// https://www.w3.org/TR/css-cascade-5/#layer-empty
class CORE_EXPORT StyleRuleLayerStatement : public StyleRuleBase {
 public:
  explicit StyleRuleLayerStatement(Vector<LayerName>&& names);
  StyleRuleLayerStatement(const StyleRuleLayerStatement& other);

  const Vector<LayerName>& GetNames() const { return names_; }
  Vector<String> GetNamesAsStrings() const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Vector<LayerName> names_;
};

class StyleRulePage : public StyleRuleGroup {
 public:
  StyleRulePage(const CSSSelectorList* selector_list,
                CSSPropertyValueSet* properties,
                HeapVector<Member<StyleRuleBase>> child_rules);
  StyleRulePage(const StyleRulePage&) = delete;

  const CSSSelector* Selector() const { return selector_list_->First(); }
  const CSSSelectorList* SelectorList() const { return selector_list_; }
  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  void WrapperAdoptSelectorList(CSSSelectorList* selectors) {
    selector_list_ = selectors;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<CSSPropertyValueSet> properties_;  // Cannot be null.
  Member<const CSSSelectorList> selector_list_;
};

class StyleRulePageMargin : public StyleRuleBase {
 public:
  StyleRulePageMargin(CSSAtRuleID id, CSSPropertyValueSet* properties);
  StyleRulePageMargin(const StyleRulePageMargin&);

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();
  CSSAtRuleID ID() const { return id_; }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  CSSAtRuleID id_;                          // What margin, e.g. @top-right.
  Member<CSSPropertyValueSet> properties_;  // Cannot be null.
};

// If you add new children of this class, remember to update IsConditionRule()
// above.
class CORE_EXPORT StyleRuleCondition : public StyleRuleGroup {
 public:
  StyleRuleCondition(const StyleRuleCondition&) = delete;
  String ConditionText() const { return condition_text_; }

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    StyleRuleGroup::TraceAfterDispatch(visitor);
  }

 protected:
  StyleRuleCondition(RuleType, HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleCondition(RuleType,
                     const String& condition_text,
                     HeapVector<Member<StyleRuleBase>> rules);
  String condition_text_;
};

class CORE_EXPORT StyleRuleMedia : public StyleRuleCondition {
 public:
  StyleRuleMedia(const MediaQuerySet*, HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleMedia(const StyleRuleMedia&) = delete;
  StyleRuleMedia(const StyleRuleMedia&,
                 HeapVector<Member<StyleRuleBase>> rules);

  const MediaQuerySet* MediaQueries() const { return media_queries_.Get(); }

  void SetMediaQueries(const MediaQuerySet* media_queries) {
    media_queries_ = media_queries;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<const MediaQuerySet> media_queries_;
};

class StyleRuleSupports : public StyleRuleCondition {
 public:
  StyleRuleSupports(const String& condition_text,
                    bool condition_is_supported,
                    HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleSupports(const StyleRuleSupports&) = delete;
  StyleRuleSupports(const StyleRuleSupports&,
                    HeapVector<Member<StyleRuleBase>> rules);

  bool ConditionIsSupported() const { return condition_is_supported_; }
  void SetConditionText(const ExecutionContext*, String);

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    StyleRuleCondition::TraceAfterDispatch(visitor);
  }

 private:
  bool condition_is_supported_;
};

class CORE_EXPORT StyleRuleContainer : public StyleRuleCondition {
 public:
  StyleRuleContainer(ContainerQuery&, HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleContainer(const StyleRuleContainer&) = delete;
  StyleRuleContainer(const StyleRuleContainer&,
                     HeapVector<Member<StyleRuleBase>> rules);

  ContainerQuery& GetContainerQuery() const { return *container_query_; }

  void SetConditionText(const ExecutionContext*,
                        StyleSheetContents* parent_contents,
                        String);

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<ContainerQuery> container_query_;
};

class StyleRuleNavigation : public StyleRuleCondition {
 public:
  StyleRuleNavigation(NavigationQuery*,
                      HeapVector<Member<StyleRuleBase>> child_rules);
  StyleRuleNavigation(const StyleRuleNavigation&) = delete;
  StyleRuleNavigation(const StyleRuleNavigation&,
                      HeapVector<Member<StyleRuleBase>>);

  void TraceAfterDispatch(Visitor*) const;

  const NavigationQuery& GetNavigationQuery() const {
    return *navigation_query_;
  }

 private:
  Member<NavigationQuery> navigation_query_;
};

class StyleRuleStartingStyle : public StyleRuleGroup {
 public:
  explicit StyleRuleStartingStyle(HeapVector<Member<StyleRuleBase>> rules);
  StyleRuleStartingStyle(const StyleRuleStartingStyle&) = delete;
  StyleRuleStartingStyle(const StyleRuleStartingStyle& other,
                         HeapVector<Member<StyleRuleBase>> rules)
      : StyleRuleStartingStyle(std::move(rules)) {}

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    StyleRuleGroup::TraceAfterDispatch(visitor);
  }
};

// This should only be used within the CSS Parser
class StyleRuleCharset : public StyleRuleBase {
 public:
  StyleRuleCharset() : StyleRuleBase(kCharset) {}
  void TraceAfterDispatch(blink::Visitor* visitor) const {
    StyleRuleBase::TraceAfterDispatch(visitor);
  }

 private:
};

// An @function rule, representing a CSS function.
class CORE_EXPORT StyleRuleFunction : public StyleRuleGroup {
 public:
  struct Parameter {
    DISALLOW_NEW();

   public:
    void Trace(blink::Visitor*) const;

    String name;
    CSSSyntaxDefinition type;
    Member<CSSVariableData> default_value;
  };

  // The body of the function is represented by `child_rules`.
  // Each child rule is either a CSSNestedDeclarations rule (holding
  // descriptors, like 'result' and local variables), or a conditional rule,
  // such as @media.
  //
  // The example below has three child rules: a CSSNestedDeclarations rule
  // holding `--x`, an @media rule, and another CSSNestedDeclarations rule
  // holding `results`.
  //
  //   @function --foo() {
  //     --x: 10px;
  //     @media (width > 100px) {
  //       --x: 20x;
  //     }
  //     result: var(--x);
  //   }
  //
  // Note: Although StyleRuleFunction itself can accommodate conditional rules,
  // it's not yet supported by parsing/evaluation.
  //
  // TODO(crbug.com/325504770): Support parsing/evaluation of conditionals.
  StyleRuleFunction(AtomicString name,
                    HeapVector<Parameter> parameters,
                    HeapVector<Member<StyleRuleBase>> child_rules,
                    CSSSyntaxDefinition return_type);
  StyleRuleFunction(const StyleRuleFunction&) = delete;

  const AtomicString& Name() const { return name_; }
  const HeapVector<Parameter>& GetParameters() const { return parameters_; }
  const CSSSyntaxDefinition& GetReturnType() const { return return_type_; }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  AtomicString name_;
  HeapVector<Parameter> parameters_;
  CSSSyntaxDefinition return_type_;
};

// An @mixin rule, representing a CSS mixin. We store all of the rules
// and declarations in child_rules_ as usual (but with their selectors
// pointing to a fake parent rule; see comment in
// CSSParserImpl::ConsumeMixinRule()), and when @apply comes,
// we clone all the children and reparent them into the point of @apply.
class CORE_EXPORT StyleRuleMixin : public StyleRuleGroup {
 public:
  StyleRuleMixin(AtomicString name,
                 HeapVector<StyleRuleFunction::Parameter> parameters,
                 HeapVector<Member<StyleRuleBase>> child_rules);
  StyleRuleMixin(const StyleRuleMixin&) = delete;
  StyleRuleMixin(const StyleRuleMixin&,
                 HeapVector<Member<StyleRuleBase>> child_rules);

  const AtomicString& GetName() const { return name_; }
  const HeapVector<StyleRuleFunction::Parameter>& GetParameters() const {
    return parameters_;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  AtomicString name_;
  HeapVector<StyleRuleFunction::Parameter> parameters_;
};

// An @apply rule, representing applying a mixin.
class CORE_EXPORT StyleRuleApplyMixin : public StyleRuleBase {
 public:
  StyleRuleApplyMixin(AtomicString name,
                      HeapVector<Member<CSSVariableData>> arguments,
                      StyleRule* fake_parent_rule_for_declarations)
      : StyleRuleBase(kApplyMixin),
        name_(std::move(name)),
        arguments_(std::move(arguments)),
        fake_parent_rule_for_declarations_(fake_parent_rule_for_declarations) {}
  StyleRuleApplyMixin(const StyleRuleMixin&) = delete;

  const AtomicString& GetName() const { return name_; }
  const HeapVector<Member<CSSVariableData>>& GetArguments() const {
    return arguments_;
  }

  // Declarations argument (for @contents). May be nullptr.
  StyleRule* FakeParentRuleForDeclarations() const {
    return fake_parent_rule_for_declarations_;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  AtomicString name_;
  HeapVector<Member<CSSVariableData>> arguments_;
  Member<StyleRule> fake_parent_rule_for_declarations_;
};

// A @contents rule, representing a placeholder within a mixin
// for rules sent in through a parameter to @apply. The @contents
// rule may have a declaration block, which is used as a fallback
// if no @contents is given. We store that declaration block
// as a dummy rule, similar to how StyleRuleMixin works.
//
// This class is named “…Statement” to avoid confusion with
// the more general concept of contents of a style rule.
class CORE_EXPORT StyleRuleContentsStatement : public StyleRuleBase {
 public:
  explicit StyleRuleContentsStatement(StyleRule* fake_parent_rule_for_fallback)
      : StyleRuleBase(kContents),
        fake_parent_rule_for_fallback_(fake_parent_rule_for_fallback) {}
  StyleRuleContentsStatement(const StyleRuleMixin&) = delete;

  // May be nullptr.
  StyleRule* FakeParentRuleForFallback() const {
    return fake_parent_rule_for_fallback_;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<StyleRule> fake_parent_rule_for_fallback_;
};

class CORE_EXPORT StyleRuleCustomMedia : public StyleRuleBase {
 public:
  StyleRuleCustomMedia(AtomicString name, MediaQuerySet* media_query_set);
  StyleRuleCustomMedia(AtomicString name, bool value);

  const String& GetName() const { return name_; }
  bool IsMediaQueryValue() const {
    return std::holds_alternative<Member<const MediaQuerySet>>(value_);
  }
  bool IsBooleanValue() const { return std::holds_alternative<bool>(value_); }
  const MediaQuerySet* GetMediaQueryValue() const {
    DCHECK(std::holds_alternative<Member<const MediaQuerySet>>(value_));
    return std::get<Member<const MediaQuerySet>>(value_);
  }
  bool GetBooleanValue() const {
    DCHECK(std::holds_alternative<bool>(value_));
    return std::get<bool>(value_);
  }
  void SetMediaQueries(const MediaQuerySet* media_queries) {
    value_ = media_queries;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  AtomicString name_;
  using CustomMediaValue = std::variant<Member<const MediaQuerySet>, bool>;
  CustomMediaValue value_;
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
struct DowncastTraits<StyleRulePageMargin> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsPageRuleMargin();
  }
};

template <>
struct DowncastTraits<StyleRuleProperty> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsPropertyRule();
  }
};

template <>
struct DowncastTraits<StyleRuleNavigation> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsNavigationRule();
  }
};

template <>
struct DowncastTraits<StyleRuleScope> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsScopeRule();
  }
};

template <>
struct DowncastTraits<StyleRuleGroup> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsMediaRule() || rule.IsSupportsRule() ||
           rule.IsContainerRule() || rule.IsLayerBlockRule() ||
           rule.IsScopeRule() || rule.IsStartingStyleRule() ||
           rule.IsFunctionRule();
  }
};

template <>
struct DowncastTraits<StyleRuleLayerBlock> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsLayerBlockRule();
  }
};

template <>
struct DowncastTraits<StyleRuleLayerStatement> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsLayerStatementRule();
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
struct DowncastTraits<StyleRuleContainer> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsContainerRule();
  }
};

template <>
struct DowncastTraits<StyleRuleCharset> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsCharsetRule();
  }
};

template <>
struct DowncastTraits<StyleRuleStartingStyle> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsStartingStyleRule();
  }
};

template <>
struct DowncastTraits<StyleRuleFunction> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsFunctionRule();
  }
};

template <>
struct DowncastTraits<StyleRuleMixin> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsMixinRule();
  }
};

template <>
struct DowncastTraits<StyleRuleApplyMixin> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsApplyMixinRule();
  }
};

template <>
struct DowncastTraits<StyleRuleContentsStatement> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsContentsRule();
  }
};

template <>
struct DowncastTraits<StyleRuleCustomMedia> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsCustomMediaRule();
  }
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::StyleRuleFunction::Parameter)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_H_
