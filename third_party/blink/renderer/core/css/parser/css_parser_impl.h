// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_

#include <memory>

#include "css_at_rule_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_source_data.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenized_value.h"
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
class StyleRulePage;
class StyleRulePositionFallback;
class StyleRuleProperty;
class StyleRuleSupports;
class StyleRuleTry;
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
    kTryRules,
    kNoRules,                // For parsing at-rules inside declaration lists
    kConditionalGroupRules,  // @media etc., see [css-conditional-3]
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
      const String&,
      bool important,
      const CSSParserContext*);
  static MutableCSSPropertyValueSet::SetResult ParseVariableValue(
      MutableCSSPropertyValueSet*,
      const AtomicString& property_name,
      const String&,
      bool important,
      const CSSParserContext*,
      bool is_animation_tainted);
  static ImmutableCSSPropertyValueSet* ParseInlineStyleDeclaration(
      const String&,
      Element*);
  static ImmutableCSSPropertyValueSet*
  ParseInlineStyleDeclaration(const String&, CSSParserMode, SecureContextMode);
  // NOTE: This function can currently only be used to parse a
  // declaration list with no nested rules, not a full style rule
  // (it is only used for things like inline style).
  static bool ParseDeclarationList(MutableCSSPropertyValueSet*,
                                   const String&,
                                   const CSSParserContext*);
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
  static CSSSelectorList* ParsePageSelector(CSSParserTokenRange,
                                            StyleSheetContents*,
                                            const CSSParserContext& context);

  static std::unique_ptr<Vector<KeyframeOffset>> ParseKeyframeKeyList(
      const CSSParserContext*,
      const String&);

  bool ConsumeSupportsDeclaration(CSSParserTokenStream&);
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

  // Consumes a value from the remaining tokens in the (possibly bounded)
  // stream.
  //
  // See also CSSParserTokenStream::Boundary.
  static CSSTokenizedValue ConsumeValue(CSSParserTokenStream&);

  static bool RemoveImportantAnnotationIfPresent(CSSTokenizedValue&);

 private:
  enum RuleListType {
    kTopLevelRuleList,
    kRegularRuleList,
    kKeyframesRuleList,
    kFontFeatureRuleList,
    kPositionFallbackRuleList,
  };

  // Returns whether the first encountered rule was valid
  template <typename T>
  bool ConsumeRuleList(CSSParserTokenStream&,
                       RuleListType,
                       CSSNestingType,
                       StyleRule* parent_rule_for_nesting,
                       T callback);

  // These functions update the range/stream they're given
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

  static StyleRuleCharset* ConsumeCharsetRule(CSSParserTokenStream&);
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
  StyleRuleBase* ConsumeScopeRule(CSSParserTokenStream&);
  StyleRuleContainer* ConsumeContainerRule(CSSParserTokenStream& stream,
                                           CSSNestingType,
                                           StyleRule* parent_rule_for_nesting);
  StyleRuleBase* ConsumeLayerRule(CSSParserTokenStream&);
  StyleRulePositionFallback* ConsumePositionFallbackRule(CSSParserTokenStream&);
  StyleRuleTry* ConsumeTryRule(CSSParserTokenStream&);

  StyleRuleKeyframe* ConsumeKeyframeStyleRule(CSSParserTokenRange prelude,
                                              const RangeOffset& prelude_offset,
                                              CSSParserTokenStream& block);
  StyleRule* ConsumeStyleRule(CSSParserTokenStream&,
                              CSSNestingType,
                              StyleRule* parent_rule_for_nesting);
  StyleRule* ConsumeStyleRuleContents(base::span<CSSSelector> selector_vector,
                                      CSSParserTokenStream& stream);

  void ConsumeDeclarationList(
      CSSParserTokenStream&,
      StyleRule::RuleType,
      CSSNestingType,
      StyleRule* parent_rule_for_nesting,
      HeapVector<Member<StyleRuleBase>, 4>* child_rules);
  // If id is absl::nullopt, we're parsing a qualified style rule;
  // otherwise, we're parsing an at-rule.
  StyleRuleBase* ConsumeNestedRule(absl::optional<CSSAtRuleID> id,
                                   CSSParserTokenStream& stream,
                                   CSSNestingType,
                                   StyleRule* parent_rule_for_nesting);
  // Returns true if a declaration was parsed and added to parsed_properties_,
  // and false otherwise.
  bool ConsumeDeclaration(CSSParserTokenStream&, StyleRule::RuleType);
  void ConsumeDeclarationValue(const CSSTokenizedValue&,
                               CSSPropertyID,
                               bool important,
                               StyleRule::RuleType);
  void ConsumeVariableValue(const CSSTokenizedValue&,
                            const AtomicString& property_name,
                            bool important,
                            bool is_animation_tainted);

  static std::unique_ptr<Vector<KeyframeOffset>> ConsumeKeyframeKeyList(
      const CSSParserContext*,
      CSSParserTokenRange);

  // Finds a previously parsed MediaQuerySet for the given `prelude_string`
  // and returns it. If no MediaQuerySet is found, parses one using `prelude`,
  // and returns the result after caching it.
  const MediaQuerySet* CachedMediaQuerySet(
      String prelude_string,
      CSSParserTokenRange prelude,
      const CSSParserTokenOffsets& offsets);

  // Create an implicit & {} rule to wrap properties in, and insert every
  // property from parsed_properties_ in it. Used when there are properties
  // directly in @media, @supports or similar (which cannot hold properties
  // by themselves, only rules; see
  // https://github.com/w3c/csswg-drafts/issues/7850).
  StyleRule* CreateImplicitNestedRule(StyleRule* parent_rule_for_nesting);

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

  HeapHashMap<String, Member<const MediaQuerySet>> media_query_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_
