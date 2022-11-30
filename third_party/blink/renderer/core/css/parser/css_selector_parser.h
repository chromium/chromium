// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SELECTOR_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SELECTOR_PARSER_H_

#include <memory>
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/arena.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenStream;
class CSSParserObserver;
class CSSSelectorList;
class Node;
class StyleSheetContents;

// SelectorVector is the list of CSS selectors as it is parsed,
// where each selector can contain others (in a tree). Typically,
// before actual use, you would convert it into a flattened list using
// CSSSelectorList::AdoptSelectorVector(), but it can be useful to have this
// temporary form to find out e.g. how many bytes it will occupy
// (e.g. in StyleRule::Create) before you actually make that allocation.
using CSSSelectorVector = Vector<ArenaUniquePtr<CSSParserSelector>>;

// FIXME: We should consider building CSSSelectors directly instead of using
// the intermediate CSSParserSelector.
class CORE_EXPORT CSSSelectorParser {
  STACK_ALLOCATED();

 public:
  // Both ParseSelector() and ConsumeSelector() return an empty list
  // on error. The arena is used for allocating the returned selectors,
  // so the return value is only valid as long as the arena is.
  // (CSSSelectorList::AdoptSelectorVector() makes new allocations,
  // which is generally what makes it possible to destroy the arena
  // quite quickly after parsing.)
  static CSSSelectorVector ParseSelector(CSSParserTokenRange,
                                         const CSSParserContext*,
                                         StyleSheetContents*,
                                         Arena&);
  static CSSSelectorVector ConsumeSelector(CSSParserTokenStream&,
                                           const CSSParserContext*,
                                           StyleSheetContents*,
                                           CSSParserObserver*,
                                           Arena&);

  static bool ConsumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

  static bool SupportsComplexSelector(CSSParserTokenRange,
                                      const CSSParserContext*);

  static CSSSelector::PseudoType ParsePseudoType(const AtomicString&,
                                                 bool has_arguments,
                                                 const Document*);
  static PseudoId ParsePseudoElement(const String&, const Node*);
  // Returns the argument of a parameterized pseudo-element. For example, for
  // '::highlight(foo)' it returns 'foo'.
  static AtomicString ParsePseudoElementArgument(const String&);

  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-start
  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-end
  //
  // Note that <scope-start> / <scope-end> are *forgiving* selector lists.
  // Therefore empty lists, represented by !CSSSelectorList::IsValid(), are
  // allowed.
  //
  // Parse errors are signalled by absl::nullopt.
  static absl::optional<CSSSelectorList> ParseScopeBoundary(
      CSSParserTokenRange,
      const CSSParserContext*,
      StyleSheetContents*);

 private:
  CSSSelectorParser(const CSSParserContext*, StyleSheetContents*, Arena&);

  // These will all consume trailing comments if successful

  CSSSelectorVector ConsumeComplexSelectorList(CSSParserTokenRange&);
  CSSSelectorVector ConsumeComplexSelectorList(CSSParserTokenStream&,
                                               CSSParserObserver*);
  CSSSelectorList ConsumeCompoundSelectorList(CSSParserTokenRange&);
  // Consumes a complex selector list if inside_compound_pseudo_ is false,
  // otherwise consumes a compound selector list.
  CSSSelectorList ConsumeNestedSelectorList(CSSParserTokenRange&);
  absl::optional<CSSSelectorList> ConsumeForgivingNestedSelectorList(
      CSSParserTokenRange&);
  // https://drafts.csswg.org/selectors/#typedef-forgiving-selector-list
  absl::optional<CSSSelectorList> ConsumeForgivingComplexSelectorList(
      CSSParserTokenRange&);
  absl::optional<CSSSelectorList> ConsumeForgivingCompoundSelectorList(
      CSSParserTokenRange&);
  // https://drafts.csswg.org/selectors/#typedef-relative-selector-list
  absl::optional<CSSSelectorList> ConsumeForgivingRelativeSelectorList(
      CSSParserTokenRange&);
  CSSSelectorList ConsumeRelativeSelectorList(CSSParserTokenRange&);

  ArenaUniquePtr<CSSParserSelector> ConsumeRelativeSelector(
      CSSParserTokenRange&);
  ArenaUniquePtr<CSSParserSelector> ConsumeComplexSelector(
      CSSParserTokenRange&);

  // ConsumePartialComplexSelector() method provides the common logic of
  // consuming a complex selector and consuming a relative selector.
  //
  // After consuming the left-most combinator of a relative selector, we can
  // consume the remaining selectors with the common logic.
  // For example, after consuming the left-most combinator '~' of the relative
  // selector '~ .a ~ .b', we can consume remaining selectors '.a ~ .b'
  // with this method.
  //
  // After consuming the left-most compound selector and a combinator of a
  // complex selector, we can also use this method to consume the remaining
  // selectors of the complex selector.
  ArenaUniquePtr<CSSParserSelector> ConsumePartialComplexSelector(
      CSSParserTokenRange&,
      CSSSelector::RelationType& /* current combinator */,
      ArenaUniquePtr<CSSParserSelector> /* previous compound selector */,
      unsigned& /* previous compound flags */);

  ArenaUniquePtr<CSSParserSelector> ConsumeCompoundSelector(
      CSSParserTokenRange&);
  // This doesn't include element names, since they're handled specially
  ArenaUniquePtr<CSSParserSelector> ConsumeSimpleSelector(CSSParserTokenRange&);

  bool ConsumeName(CSSParserTokenRange&,
                   AtomicString& name,
                   AtomicString& namespace_prefix);

  // These will return nullptr when the selector is invalid
  ArenaUniquePtr<CSSParserSelector> ConsumeId(CSSParserTokenRange&);
  ArenaUniquePtr<CSSParserSelector> ConsumeClass(CSSParserTokenRange&);
  ArenaUniquePtr<CSSParserSelector> ConsumePseudo(CSSParserTokenRange&);
  ArenaUniquePtr<CSSParserSelector> ConsumeAttribute(CSSParserTokenRange&);

  CSSSelector::RelationType ConsumeCombinator(CSSParserTokenRange&);
  CSSSelector::MatchType ConsumeAttributeMatch(CSSParserTokenRange&);
  CSSSelector::AttributeMatchType ConsumeAttributeFlags(CSSParserTokenRange&);

  const AtomicString& DefaultNamespace() const;
  const AtomicString& DetermineNamespace(const AtomicString& prefix);
  void PrependTypeSelectorIfNeeded(const AtomicString& namespace_prefix,
                                   bool has_element_name,
                                   const AtomicString& element_name,
                                   CSSParserSelector*);
  static ArenaUniquePtr<CSSParserSelector> AddSimpleSelectorToCompound(
      Arena& arena,
      ArenaUniquePtr<CSSParserSelector> compound_selector,
      ArenaUniquePtr<CSSParserSelector> simple_selector);
  static ArenaUniquePtr<CSSParserSelector>
  SplitCompoundAtImplicitShadowCrossingCombinator(
      ArenaUniquePtr<CSSParserSelector> compound_selector);
  void RecordUsageAndDeprecations(const CSSSelectorVector&);
  static bool ContainsUnknownWebkitPseudoElements(
      const CSSSelector& complex_selector);

  void SetInSupportsParsing() { in_supports_parsing_ = true; }

  const CSSParserContext* context_;
  const StyleSheetContents* style_sheet_;

  bool failed_parsing_ = false;
  bool disallow_pseudo_elements_ = false;
  // If we're inside a pseudo class that only accepts compound selectors,
  // for example :host, inner :is()/:where() pseudo classes are also only
  // allowed to contain compound selectors.
  bool inside_compound_pseudo_ = false;
  // When parsing a compound which includes a pseudo-element, the simple
  // selectors permitted to follow that pseudo-element may be restricted.
  // If this is the case, then restricting_pseudo_element_ will be set to the
  // PseudoType of the pseudo-element causing the restriction.
  CSSSelector::PseudoType restricting_pseudo_element_ =
      CSSSelector::kPseudoUnknown;
  // If we're _resisting_ the default namespace, it means that we are inside
  // a nested selector (:is(), :where(), etc) where we should _consider_
  // ignoring the default namespace (depending on circumstance). See the
  // relevant spec text [1] regarding default namespaces for information about
  // those circumstances.
  //
  // [1] https://drafts.csswg.org/selectors/#matches
  bool resist_default_namespace_ = false;
  // While this flag is true, the default namespace is ignored. In other words,
  // the default namespace is '*' while this flag is true.
  bool ignore_default_namespace_ = false;

  // The 'found_pseudo_in_has_argument_' flag is true when we found any pseudo
  // in :has() argument while parsing.
  bool found_pseudo_in_has_argument_ = false;
  bool is_inside_has_argument_ = false;

  // The 'found_complex_logical_combinations_in_has_argument_' flag is true when
  // we found any logical combinations (:is(), :where(), :not()) containing
  // complex selector in :has() argument while parsing.
  bool found_complex_logical_combinations_in_has_argument_ = false;
  bool is_inside_logical_combination_in_has_argument_ = false;

  bool in_supports_parsing_ = false;

  // Used for temporary allocations of CSSParserSelector; anytime we have
  // an ArenaUniquePtr<CSSParserSelector>, they are allocated on this arena.
  // (They do not escape the class; they are generally discarded after
  // construction, as they are converted into longer-lived CSSSelectorVector
  // objects.)
  Arena& arena_;

  class DisallowPseudoElementsScope {
    STACK_ALLOCATED();

   public:
    explicit DisallowPseudoElementsScope(CSSSelectorParser* parser)
        : parser_(parser), was_disallowed_(parser_->disallow_pseudo_elements_) {
      parser_->disallow_pseudo_elements_ = true;
    }
    DisallowPseudoElementsScope(const DisallowPseudoElementsScope&) = delete;
    DisallowPseudoElementsScope& operator=(const DisallowPseudoElementsScope&) =
        delete;

    ~DisallowPseudoElementsScope() {
      parser_->disallow_pseudo_elements_ = was_disallowed_;
    }

   private:
    CSSSelectorParser* parser_;
    bool was_disallowed_;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SELECTOR_PARSER_H_
