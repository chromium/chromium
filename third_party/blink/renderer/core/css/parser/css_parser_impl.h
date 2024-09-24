// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_

#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "css_at_rule_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_source_data.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSLazyParsingState;
class CSSParserContext;
class CSSParserObserver;
class CSSParserTokenStream;
class StyleRule;
class StyleRuleViewTransition;
class StyleRuleBase;
class StyleRuleCharset;
class StyleRuleCounterStyle;
class StyleRuleFontFace;
class StyleRuleFontPaletteValues;
class StyleRuleFontFeatureValues;
class StyleRuleFontFeature;
class StyleRuleImport;
class StyleRuleKeyframe;
class StyleRuleKeyframes;
class StyleRuleMedia;
class StyleRuleNamespace;
class StyleRuleNestedDeclarations;
class StyleRulePage;
class StyleRulePositionTry;
class StyleRuleProperty;
class StyleRuleSupports;
class StyleSheetContents;
class Element;

enum class ParseSheetResult {
  kSucceeded,
  kHasUnallowedImportRule,
};

class CORE_EXPORT CSSParserImpl {
  STACK_ALLOCATED();

 public:
  explicit CSSParserImpl(const CSSParserContext*,
                         StyleSheetContents* = nullptr);
  CSSParserImpl(const CSSParserImpl&) = delete;
  CSSParserImpl& operator=(const CSSParserImpl&) = delete;

  enum AllowedRulesType {
    // As per css-syntax, css-cascade and css-namespaces, @charset rules
    // must come first, followed by @layer, @import then @namespace.
    // AllowImportRules actually means we allow @import and any rules that
    // may follow it, i.e. @namespace rules and regular rules.
    // AllowCharsetRules and AllowNamespaceRules behave similarly.
    kAllowCharsetRules,
    kAllowLayerStatementRules,
    kAllowImportRules,
    kAllowNamespaceRules,
    kRegularRules,
    kKeyframeRules,
    kFontFeatureRules,
    // For parsing at-rules inside declaration lists.
    kNoRules,
    // https://drafts.csswg.org/css-nesting/#nested-group-rules
    kNestedGroupRules,
    // https://www.w3.org/TR/css-page-3/#syntax-page-selector
    kPageMarginRules,
  };

  // Represents the start and end offsets of a CSSParserTokenRange.
  struct RangeOffset {
    wtf_size_t start, end;

    RangeOffset(wtf_size_t start, wtf_size_t end) : start(start), end(end) {
      DCHECK(start <= end);
    }

    // Used when we don't care what the offset is (typically when we don't have
    // an observer).
    static RangeOffset Ignore() { return {0, 0}; }
  };

  static MutableCSSPropertyValueSet::SetResult ParseValue(
      MutableCSSPropertyValueSet*,
      CSSPropertyID,
      StringView,
      bool important,
      const CSSParserContext*);
  static MutableCSSPropertyValueSet::SetResult ParseVariableValue(
      MutableCSSPropertyValueSet*,
      const AtomicString& property_name,
      StringView,
      bool important,
      const CSSParserContext*,
      bool is_animation_tainted);
  static ImmutableCSSPropertyValueSet* ParseInlineStyleDeclaration(
      const String&,
      Element*);
  static ImmutableCSSPropertyValueSet* ParseInlineStyleDeclaration(
      const String&,
      CSSParserMode,
      SecureContextMode,
      const Document*);
  // NOTE: This function can currently only be used to parse a
  // declaration list with no nested rules, not a full style rule
  // (it is only used for things like inline style).
  static bool ParseDeclarationList(MutableCSSPropertyValueSet*,
                                   const String&,
                                   const CSSParserContext*);
  // This is used for parsing CSSNestedDeclarations from ParseRuleForInsert
  // (CSSGroupingRule/CSSStyleRule.insertRule).
  static StyleRuleBase* ParseNestedDeclarationsRule(
      const CSSParserContext*,
      CSSNestingType,
      StyleRule* parent_rule_for_nesting,
      StringView);
  static StyleRuleBase* ParseRule(const String&,
                                  const CSSParserContext*,
                                  CSSNestingType,
                                  StyleRule* parent_rule_for_nesting,
                                  StyleSheetContents*,
                                  AllowedRulesType);
  static ParseSheetResult ParseStyleSheet(
      const String&,
      const CSSParserContext*,
      StyleSheetContents*,
      CSSDeferPropertyParsing = CSSDeferPropertyParsing::kNo,
      bool allow_import_rules = true);
  static CSSSelectorList* ParsePageSelector(CSSParserTokenStream&,
                                            StyleSheetContents*,
                                            const CSSParserContext& context);

  static std::unique_ptr<Vector<KeyframeOffset>> ParseKeyframeKeyList(
      const CSSParserContext*,
      const String&);
  static String ParseCustomPropertyName(StringView name_text);

  bool ConsumeEndOfPreludeForAtRuleWithoutBlock(CSSParserTokenStream& stream,
                                                CSSAtRuleID id);
  bool ConsumeEndOfPreludeForAtRuleWithBlock(CSSParserTokenStream& stream,
                                             CSSAtRuleID id);
  bool ConsumeSupportsDeclaration(CSSParserTokenStream&);
  void ConsumeErroneousAtRule(CSSParserTokenStream& stream, CSSAtRuleID id);
  const CSSParserContext* GetContext() const { return context_; }

  static void ParseDeclarationListForInspector(const String&,
                                               const CSSParserContext*,
                                               CSSParserObserver&);
  static void ParseStyleSheetForInspector(const String&,
                                          const CSSParserContext*,
                                          StyleSheetContents*,
                                          CSSParserObserver&);

  static CSSPropertyValueSet* ParseDeclarationListForLazyStyle(
      const String&,
      wtf_size_t offset,
      const CSSParserContext*);

  CSSParserMode GetMode() const;

 private:
  friend class TestCSSParserImpl;

  enum RuleListType {
    kTopLevelRuleList,
    kRegularRuleList,
    kKeyframesRuleList,
    kFontFeatureRuleList,
  };

  // Returns whether the first encountered rule was valid
  template <typename T>
  bool ConsumeRuleList(CSSParserTokenStream&,
                       RuleListType,
                       CSSNestingType,
                       StyleRule* parent_rule_for_nesting,
                       T callback);

  // These functions update the stream they're given
  StyleRuleBase* ConsumeAtRule(CSSParserTokenStream&,
                               AllowedRulesType,
                               CSSNestingType,
                               StyleRule* parent_rule_for_nesting);
  StyleRuleBase* ConsumeAtRuleContents(CSSAtRuleID id,
                                       CSSParserTokenStream& stream,
                                       AllowedRulesType allowed_rules,
                                       CSSNestingType,
                                       StyleRule* parent_rule_for_nesting);
  StyleRuleBase* ConsumeQualifiedRule(CSSParserTokenStream&,
                                      AllowedRulesType,
                                      CSSNestingType,
                                      StyleRule* parent_rule_for_nesting);

  StyleRulePageMargin* ConsumePageMarginRule(CSSAtRuleID rule_id,
                                             CSSParserTokenStream& stream);
  StyleRuleCharset* ConsumeCharsetRule(CSSParserTokenStream&);
  StyleRuleImport* ConsumeImportRule(const AtomicString& prelude_uri,
                                     CSSParserTokenStream&);
  StyleRuleNamespace* ConsumeNamespaceRule(CSSParserTokenStream&);
  StyleRuleMedia* ConsumeMediaRule(CSSParserTokenStream& stream,
                                   CSSNestingType,
                                   StyleRule* parent_rule_for_nesting);
  StyleRuleSupports* ConsumeSupportsRule(CSSParserTokenStream& stream,
                                         CSSNestingType,
                                         StyleRule* parent_rule_for_nesting);
  StyleRuleStartingStyle* ConsumeStartingStyleRule(
      CSSParserTokenStream& stream,
      CSSNestingType,
      StyleRule* parent_rule_for_nesting);
  StyleRuleFontFace* ConsumeFontFaceRule(CSSParserTokenStream&);
  StyleRuleFontPaletteValues* ConsumeFontPaletteValuesRule(
      CSSParserTokenStream&);
  StyleRuleFontFeatureValues* ConsumeFontFeatureValuesRule(
      CSSParserTokenStream&);
  StyleRuleFontFeature* ConsumeFontFeatureRule(CSSAtRuleID,
                                               CSSParserTokenStream&);
  StyleRuleKeyframes* ConsumeKeyframesRule(bool webkit_prefixed,
                                           CSSParserTokenStream&);
  StyleRulePage* ConsumePageRule(CSSParserTokenStream&);
  StyleRuleProperty* ConsumePropertyRule(CSSParserTokenStream&);
  StyleRuleCounterStyle* ConsumeCounterStyleRule(CSSParserTokenStream&);
  StyleRuleBase* ConsumeScopeRule(CSSParserTokenStream&,
                                  CSSNestingType,
                                  StyleRule* parent_rule_for_nesting);
  StyleRuleViewTransition* ConsumeViewTransitionRule(
      CSSParserTokenStream& stream);
  StyleRuleContainer* ConsumeContainerRule(CSSParserTokenStream& stream,
                                           CSSNestingType,
                                           StyleRule* parent_rule_for_nesting);
  StyleRuleBase* ConsumeLayerRule(CSSParserTokenStream&,
                                  CSSNestingType,
                                  StyleRule* parent_rule_for_nesting);
  StyleRulePositionTry* ConsumePositionTryRule(CSSParserTokenStream&);

  StyleRuleFunction* ConsumeFunctionRule(CSSParserTokenStream& stream);
  std::optional<Vector<StyleRuleFunction::Parameter>> ConsumeFunctionParameters(
      CSSParserTokenStream& stream);
  StyleRuleMixin* ConsumeMixinRule(CSSParserTokenStream& stream);
  StyleRuleApplyMixin* ConsumeApplyMixinRule(CSSParserTokenStream& stream);

  StyleRuleKeyframe* ConsumeKeyframeStyleRule(
      std::unique_ptr<Vector<KeyframeOffset>> key_list,
      const RangeOffset& prelude_offset,
      CSSParserTokenStream& block);

  // https://drafts.csswg.org/css-syntax/#consume-a-qualified-rule
  //
  // - CSSNestingType determines which implicit selector to insert for relative
  //   selectors ('&' for kNesting, and ':scope' for kScope).
  // - `parent_rule_for_nesting` determines what '&' points to.
  // - `nested` refers to the "nested" flag referenced by the linked
  //    parser algorithm.
  // - `invalid_rule_error` (output parameter) is set when the selector list
  //   didn't parse, but we have a valid rule otherwise.
  StyleRule* ConsumeStyleRule(CSSParserTokenStream&,
                              CSSNestingType,
                              StyleRule* parent_rule_for_nesting,
                              bool nested,
                              bool& invalid_rule_error);
  StyleRule* ConsumeStyleRuleContents(base::span<CSSSelector> selector_vector,
                                      CSSParserTokenStream& stream);

  void ConsumeDeclarationList(
      CSSParserTokenStream&,
      StyleRule::RuleType,
      CSSNestingType,
      StyleRule* parent_rule_for_nesting,
      wtf_size_t nested_declarations_start_index,
      HeapVector<Member<StyleRuleBase>, 4>* child_rules);

  void ConsumeRuleListOrNestedDeclarationList(
      CSSParserTokenStream&,
      bool is_nested_group_rule,
      CSSNestingType,
      StyleRule* parent_rule_for_nesting,
      HeapVector<Member<StyleRuleBase>, 4>* child_rules);

  // If id is std::nullopt, we're parsing a qualified style rule;
  // otherwise, we're parsing an at-rule.
  StyleRuleBase* ConsumeNestedRule(std::optional<CSSAtRuleID> id,
                                   StyleRule::RuleType parent_rule_type,
                                   CSSParserTokenStream& stream,
                                   CSSNestingType,
                                   StyleRule* parent_rule_for_nesting,
                                   bool& invalid_rule_error);

  // Returns true if a declaration was parsed and added to parsed_properties_,
  // and false otherwise.
  bool ConsumeDeclaration(CSSParserTokenStream&, StyleRule::RuleType);
  void ConsumeDeclarationValue(CSSParserTokenStream&,
                               CSSPropertyID,
                               bool is_in_declaration_list,
                               StyleRule::RuleType);
  bool ConsumeVariableValue(CSSParserTokenStream& stream,
                            const AtomicString& property_name,
                            bool allow_important_annotation,
                            bool is_animation_tainted);

  static std::unique_ptr<Vector<KeyframeOffset>> ConsumeKeyframeKeyList(
      const CSSParserContext*,
      CSSParserTokenStream&);

  // Create an implicit & {} rule to wrap properties in, and insert every
  // property from parsed_properties_ in it. Used when there are properties
  // directly in @media, @supports or similar (which cannot hold properties
  // by themselves, only rules; see
  // https://github.com/w3c/csswg-drafts/issues/7850).
  //
  // If CSSNestingType::kScope is provided, an implicit :scope {} rule
  // is created instead.
  StyleRule* CreateImplicitNestedRule(CSSNestingType,
                                      StyleRule* parent_rule_for_nesting);

  // CSSNestedDeclarations
  // =====================
  //
  // Bare declarations that appear after a nested child rule
  // must be wrapped in CSSNestedDeclarations rules [1]. For example:
  //
  //  .a {
  //    color: green;
  //    .b { }
  //    width: 100px;
  //    height: 100px;
  //    div { }
  //    opacity: 1;
  //  }
  //
  // Must be wrapped as follows:
  //
  //  .a {
  //    color: green;
  //    .b { }
  //    CSSNestedDeclarations {
  //      width: 100px;
  //      height: 100px;
  //    }
  //    div { }
  //    CSSNestedDeclarations {
  //      opacity: 1;
  //    }
  //  }
  //
  //
  // We implement this by tracking the start index (into parsed_properties_)
  // of these bare declaration segments. Whenever we successfully parse
  // a child rule, we store the current number of parsed_properties_
  // as the start index. Then, when we encounter the *next* child rule
  // (or the end of the parent block), we create a CSSNestedDeclarations rule
  // wrapping the declarations in the range [start_index, end).
  //
  // [1] https://drafts.csswg.org/css-nesting-1/#nested-declarations-rule

  // Creates a new "nested declarations rule", consisting of the declarations
  // (parsed_properties_) in the range [start_index, end_index).
  // The parsed properties in the range are left as-is, i.e. not removed
  // from parsed_properties_.
  //
  // https://drafts.csswg.org/css-nesting-1/#nested-declarations-rule
  StyleRuleNestedDeclarations* CreateNestedDeclarationsRule(
      CSSNestingType nesting_type,
      const CSSSelector* selector_list,
      wtf_size_t start_index,
      wtf_size_t end_index);

  // Adds a new "nested declarations rule" to child_rules, consisting of
  // the declarations (parsed_properties_) from start_index until the end.
  // The affected declarations (if any) are removed from parsed_properties_.
  // See also the "CSSNestedDeclarations" comment above for more information
  // on what this is used for.
  void EmitNestedDeclarationsRuleIfNeeded(
      CSSNestingType,
      StyleRule* parent_rule_for_nesting,
      wtf_size_t start_index,
      HeapVector<Member<StyleRuleBase>, 4>& child_rules);

  // FIXME: Can we build CSSPropertyValueSets directly?
  HeapVector<CSSPropertyValue, 64> parsed_properties_;

  const CSSParserContext* context_;
  StyleSheetContents* style_sheet_;

  // For the inspector
  CSSParserObserver* observer_;

  CSSLazyParsingState* lazy_state_;

  // Used for temporary allocations of CSSParserSelector (we send it down
  // to CSSSelectorParser, which temporarily holds on to a reference to it).
  HeapVector<CSSSelector> arena_;

  // True when parsing a StyleRule via ConsumeNestedRule.
  bool in_nested_style_rule_ = false;

  // True if we're within the body of an @scope rule. While this is true,
  // any selectors parsed will gain kScopeActivations as needed.
  bool is_within_scope_ = false;

  HeapHashMap<String, Member<const MediaQuerySet>> media_query_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_
