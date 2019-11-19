// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_source_data.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
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
class StyleRuleFontFace;
class StyleRuleImport;
class StyleRuleKeyframe;
class StyleRuleKeyframes;
class StyleRuleMedia;
class StyleRuleNamespace;
class StyleRulePage;
class StyleRuleProperty;
class StyleRuleSupports;
class StyleRuleViewport;
class StyleSheetContents;
class Element;

enum class ParseSheetResult {
  kSucceeded,
  kHasUnallowedImportRule,
};

class CSSParserImpl {
  STACK_ALLOCATED();

 public:
  CSSParserImpl(const CSSParserContext*, StyleSheetContents* = nullptr);

  enum AllowedRulesType {
    // As per css-syntax, css-cascade and css-namespaces, @charset rules
    // must come first, followed by @import then @namespace.
    // AllowImportRules actually means we allow @import and any rules thay
    // may follow it, i.e. @namespace rules and regular rules.
    // AllowCharsetRules and AllowNamespaceRules behave similarly.
    kAllowCharsetRules,
    kAllowImportRules,
    kAllowNamespaceRules,
    kRegularRules,
    kKeyframeRules,
    kFontFeatureRules,
    kNoRules,  // For parsing at-rules inside declaration lists
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
  static bool ParseDeclarationList(MutableCSSPropertyValueSet*,
                                   const String&,
                                   const CSSParserContext*);
  static StyleRuleBase* ParseRule(const String&,
                                  const CSSParserContext*,
                                  StyleSheetContents*,
                                  AllowedRulesType);
  static ParseSheetResult ParseStyleSheet(
      const String&,
      const CSSParserContext*,
      StyleSheetContents*,
      CSSDeferPropertyParsing = CSSDeferPropertyParsing::kNo,
      bool allow_import_rules = true);
  static CSSSelectorList ParsePageSelector(CSSParserTokenRange,
                                           StyleSheetContents*);

  static std::unique_ptr<Vector<double>> ParseKeyframeKeyList(const String&);

  bool SupportsDeclaration(CSSParserTokenRange&);

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

 private:
  enum RuleListType {
    kTopLevelRuleList,
    kRegularRuleList,
    kKeyframesRuleList,
    kFontFeatureRuleList,
  };

  // Returns whether the first encountered rule was valid
  template <typename T>
  bool ConsumeRuleList(CSSParserTokenStream&, RuleListType, T callback);

  // These functions update the range/stream they're given
  StyleRuleBase* ConsumeAtRule(CSSParserTokenStream&, AllowedRulesType);
  StyleRuleBase* ConsumeQualifiedRule(CSSParserTokenStream&, AllowedRulesType);

  static StyleRuleCharset* ConsumeCharsetRule(CSSParserTokenRange prelude);
  StyleRuleImport* ConsumeImportRule(AtomicString prelude_uri,
                                     CSSParserTokenRange prelude,
                                     const RangeOffset& prelude_offset);
  StyleRuleNamespace* ConsumeNamespaceRule(CSSParserTokenRange prelude);
  StyleRuleMedia* ConsumeMediaRule(CSSParserTokenRange prelude,
                                   const RangeOffset& prelude_offset,
                                   CSSParserTokenStream& block);
  StyleRuleSupports* ConsumeSupportsRule(CSSParserTokenRange prelude,
                                         const RangeOffset& prelude_offset,
                                         CSSParserTokenStream& block);
  StyleRuleViewport* ConsumeViewportRule(CSSParserTokenRange prelude,
                                         const RangeOffset& prelude_offset,
                                         CSSParserTokenStream& block);
  StyleRuleFontFace* ConsumeFontFaceRule(CSSParserTokenRange prelude,
                                         const RangeOffset& prelude_offset,
                                         CSSParserTokenStream& block);
  StyleRuleKeyframes* ConsumeKeyframesRule(bool webkit_prefixed,
                                           CSSParserTokenRange prelude,
                                           const RangeOffset& prelude_offset,
                                           CSSParserTokenStream& block);
  StyleRulePage* ConsumePageRule(CSSParserTokenRange prelude,
                                 const RangeOffset& prelude_offset,
                                 CSSParserTokenStream& block);
  StyleRuleProperty* ConsumePropertyRule(CSSParserTokenRange prelude,
                                         const RangeOffset& prelude_offset,
                                         CSSParserTokenStream& block);

  StyleRuleKeyframe* ConsumeKeyframeStyleRule(CSSParserTokenRange prelude,
                                              const RangeOffset& prelude_offset,
                                              CSSParserTokenStream& block);
  StyleRule* ConsumeStyleRule(CSSParserTokenStream&);

  void ConsumeDeclarationList(CSSParserTokenStream&, StyleRule::RuleType);
  void ConsumeDeclaration(CSSParserTokenRange,
                          const RangeOffset& decl_offset,
                          StyleRule::RuleType);
  void ConsumeDeclarationValue(CSSParserTokenRange,
                               CSSPropertyID,
                               bool important,
                               StyleRule::RuleType);
  void ConsumeVariableValue(CSSParserTokenRange,
                            const AtomicString& property_name,
                            bool important,
                            bool is_animation_tainted);

  static std::unique_ptr<Vector<double>> ConsumeKeyframeKeyList(
      CSSParserTokenRange);

  // FIXME: Can we build CSSPropertyValueSets directly?
  // FIXME: Investigate using a smaller inline buffer
  HeapVector<CSSPropertyValue, 256> parsed_properties_;

  Member<const CSSParserContext> context_;
  Member<StyleSheetContents> style_sheet_;

  // For the inspector
  CSSParserObserver* observer_;

  Member<CSSLazyParsingState> lazy_state_;
  DISALLOW_COPY_AND_ASSIGN(CSSParserImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_IMPL_H_
