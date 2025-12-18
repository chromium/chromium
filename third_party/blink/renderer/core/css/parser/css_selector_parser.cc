// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"

#include <algorithm>
#include <memory>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/link_condition_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static void RecordUsageAndDeprecationsOneSelector(
    const CSSSelector* selector,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    bool* has_visited_pseudo);

namespace {

bool AtEndIgnoringWhitespace(CSSParserTokenStream& stream) {
  stream.EnsureLookAhead();
  CSSParserSavePoint savepoint(stream);
  stream.ConsumeWhitespace();
  return stream.AtEnd();
}

bool IsHostPseudoSelector(const CSSSelector& selector) {
  return selector.GetPseudoType() == CSSSelector::kPseudoHost ||
         selector.GetPseudoType() == CSSSelector::kPseudoHostContext;
}

// Some pseudo-elements behave as if they have an implicit combinator to their
// left even though they are written without one. This method returns the
// correct implicit combinator. If no new combinator should be used,
// it returns RelationType::kSubSelector.
CSSSelector::RelationType GetImplicitCombinatorForMatching(
    CSSSelector::PseudoType pseudo_type) {
  switch (pseudo_type) {
    case CSSSelector::PseudoType::kPseudoSlotted:
      return CSSSelector::RelationType::kShadowSlot;
    case CSSSelector::PseudoType::kPseudoWebKitCustomElement:
    case CSSSelector::PseudoType::kPseudoBlinkInternalElement:
    case CSSSelector::PseudoType::kPseudoCue:
    case CSSSelector::PseudoType::kPseudoDetailsContent:
    case CSSSelector::PseudoType::kPseudoPlaceholder:
    case CSSSelector::PseudoType::kPseudoFileSelectorButton:
    case CSSSelector::PseudoType::kPseudoPicker:
    case CSSSelector::PseudoType::kPseudoPermissionIcon:
      return CSSSelector::RelationType::kUAShadow;
    case CSSSelector::PseudoType::kPseudoPart:
      return CSSSelector::RelationType::kShadowPart;
    case CSSSelector::PseudoType::kPseudoBefore:
    case CSSSelector::PseudoType::kPseudoAfter:
    case CSSSelector::PseudoType::kPseudoMarker:
      // TODO(crbug.com/444386484): Support additional pseudo-elements.
      if (RuntimeEnabledFeatures::CSSLogicalCombinationPseudoEnabled()) {
        return CSSSelector::RelationType::kPseudoChild;
      }
      [[fallthrough]];
    default:
      return CSSSelector::RelationType::kSubSelector;
  }
}

bool NeedsImplicitCombinatorForMatching(const CSSSelector& selector) {
  return GetImplicitCombinatorForMatching(selector.GetPseudoType()) !=
         CSSSelector::RelationType::kSubSelector;
}

// Marks the end of parsing a complex selector. (In many cases, there may
// be more complex selectors after this, since we are often dealing with
// lists of complex selectors. Those are marked using SetLastInSelectorList(),
// which happens in CSSSelectorList::AdoptSelectorVector.)
void MarkAsEntireComplexSelector(base::span<CSSSelector> selectors) {
#if DCHECK_IS_ON()
  for (CSSSelector& selector : selectors.first(selectors.size() - 1)) {
    DCHECK(!selector.IsLastInComplexSelector());
  }
#endif
  selectors.back().SetLastInComplexSelector(true);
}

// https://drafts.csswg.org/css-overflow-5/#typedef-scroll-button-direction
bool IsScrollButtonDirectionKeyword(const CSSParserToken& ident) {
  switch (ident.Id()) {
    case CSSValueID::kUp:
    case CSSValueID::kDown:
    case CSSValueID::kLeft:
    case CSSValueID::kRight:
    case CSSValueID::kBlockStart:
    case CSSValueID::kBlockEnd:
    case CSSValueID::kInlineStart:
    case CSSValueID::kInlineEnd:
      return true;
    default:
      return false;
  }
}

}  // namespace

// static
base::span<CSSSelector> CSSSelectorParser::ParseSelector(
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    bool semicolon_aborts_nested_selector,
    StyleSheetContents* style_sheet,
    HeapVector<CSSSelector>& arena) {
  CSSSelectorParser parser(context, parent_rule_for_nesting,
                           semicolon_aborts_nested_selector, style_sheet,
                           arena);
  stream.ConsumeWhitespace();
  ResultFlags result_flags = 0;
  base::span<CSSSelector> result =
      parser.ConsumeComplexSelectorList(stream, nesting_type, result_flags);
  if (!stream.AtEnd()) {
    return {};
  }

  parser.RecordUsageAndDeprecations(result, nesting_type);
  return result;
}

// static
base::span<CSSSelector> CSSSelectorParser::ConsumeSelector(
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    bool semicolon_aborts_nested_selector,
    StyleSheetContents* style_sheet,
    CSSParserObserver* observer,
    HeapVector<CSSSelector>& arena,
    bool* has_visited_style) {
  CSSSelectorParser parser(context, parent_rule_for_nesting,
                           semicolon_aborts_nested_selector, style_sheet,
                           arena);
  stream.ConsumeWhitespace();
  ResultFlags result_flags = 0;
  base::span<CSSSelector> result = parser.ConsumeComplexSelectorList(
      stream, observer, nesting_type, result_flags);
  parser.RecordUsageAndDeprecations(result, nesting_type, has_visited_style);
  return result;
}

// static
base::span<CSSSelector> CSSSelectorParser::ParseScopeBoundary(
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    StyleSheetContents* style_sheet,
    HeapVector<CSSSelector>& arena) {
  CSSSelectorParser parser(context, parent_rule_for_nesting,
                           /*semicolon_aborts_nested_selector=*/false,
                           style_sheet, arena);
  DisallowPseudoElementsScope disallow_pseudo_elements(&parser);

  stream.ConsumeWhitespace();
  ResultFlags result_flags = 0;
  base::span<CSSSelector> result =
      parser.ConsumeComplexSelectorList(stream, nesting_type, result_flags);
  if (result.empty() || !stream.AtEnd()) {
    return {};
  }
  parser.RecordUsageAndDeprecations(result, nesting_type);
  return result;
}

// static
bool CSSSelectorParser::SupportsComplexSelector(
    CSSParserTokenStream& stream,
    const CSSParserContext* context) {
  stream.ConsumeWhitespace();
  HeapVector<CSSSelector> arena;
  CSSSelectorParser parser(context, /*parent_rule_for_nesting=*/nullptr,
                           /*semicolon_aborts_nested_selector=*/false, nullptr,
                           arena);
  parser.SetInSupportsParsing();
  ResultFlags result_flags = 0;
  base::span<CSSSelector> selectors = parser.ConsumeComplexSelector(
      stream, CSSNestingType::kNone,
      /*first_in_complex_selector_list=*/true, result_flags);
  if (parser.failed_parsing_ || !stream.AtEnd() || selectors.empty()) {
    return false;
  }
  if (ContainsUnknownWebkitPseudoElements(selectors)) {
    return false;
  }
  return true;
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext* context,
                                     const StyleRule* parent_rule_for_nesting,
                                     bool semicolon_aborts_nested_selector,
                                     StyleSheetContents* style_sheet,
                                     HeapVector<CSSSelector>& output)
    : context_(context),
      parent_rule_for_nesting_(parent_rule_for_nesting),
      semicolon_aborts_nested_selector_(semicolon_aborts_nested_selector),
      style_sheet_(style_sheet),
      output_(output) {}

base::span<CSSSelector> CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    ResultFlags& result_flags) {
  ResetVectorAfterScope reset_vector(output_);
  if (ConsumeComplexSelector(stream, nesting_type,
                             /*first_in_complex_selector_list=*/true,
                             result_flags)
          .empty()) {
    return {};
  }
  while (!stream.AtEnd() && stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeIncludingWhitespace();
    if (ConsumeComplexSelector(stream, nesting_type,
                               /*first_in_complex_selector_list=*/false,
                               result_flags)
            .empty()) {
      return {};
    }
  }

  if (failed_parsing_) {
    return {};
  }

  return reset_vector.CommitAddedElements();
}

static bool AtEndOfComplexSelector(CSSParserTokenStream& stream) {
  const CSSParserToken& token = stream.Peek();
  return stream.AtEnd() || token.GetType() == kLeftBraceToken ||
         token.GetType() == kCommaToken;
}

base::span<CSSSelector> CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenStream& stream,
    CSSParserObserver* observer,
    CSSNestingType nesting_type,
    ResultFlags& result_flags) {
  ResetVectorAfterScope reset_vector(output_);

  bool first_in_complex_selector_list = true;
  while (true) {
    const wtf_size_t selector_offset_start = stream.LookAheadOffset();

    if (ConsumeComplexSelector(stream, nesting_type,
                               first_in_complex_selector_list, result_flags)
            .empty() ||
        failed_parsing_ || !AtEndOfComplexSelector(stream)) {
      if (AbortsNestedSelectorParsing(kSemicolonToken,
                                      semicolon_aborts_nested_selector_,
                                      nesting_type)) {
        stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kCommaToken,
                                     kSemicolonToken>();
      } else {
        stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kCommaToken>();
      }
      return {};
    }
    const wtf_size_t selector_offset_end = stream.LookAheadOffset();
    first_in_complex_selector_list = false;

    if (observer) {
      observer->ObserveSelector(selector_offset_start, selector_offset_end);
    }

    if (stream.UncheckedAtEnd()) {
      break;
    }

    if (stream.Peek().GetType() == kLeftBraceToken ||
        AbortsNestedSelectorParsing(stream.Peek().GetType(),
                                    semicolon_aborts_nested_selector_,
                                    nesting_type)) {
      break;
    }

    DCHECK_EQ(stream.Peek().GetType(), kCommaToken);
    stream.ConsumeIncludingWhitespace();
  }

  return reset_vector.CommitAddedElements();
}

CSSSelectorList* CSSSelectorParser::ConsumeCompoundSelectorList(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  ResetVectorAfterScope reset_vector(output_);

  base::span<CSSSelector> selector =
      ConsumeCompoundSelector(stream, CSSNestingType::kNone, result_flags);
  stream.ConsumeWhitespace();
  if (selector.empty()) {
    return nullptr;
  }
  MarkAsEntireComplexSelector(selector);
  while (!stream.AtEnd() && stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeIncludingWhitespace();
    selector =
        ConsumeCompoundSelector(stream, CSSNestingType::kNone, result_flags);
    stream.ConsumeWhitespace();
    if (selector.empty()) {
      return nullptr;
    }
    MarkAsEntireComplexSelector(selector);
  }

  if (failed_parsing_) {
    return nullptr;
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

CSSSelectorList* CSSSelectorParser::ConsumeNestedSelectorList(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  if (inside_compound_pseudo_) {
    return ConsumeCompoundSelectorList(stream, result_flags);
  }

  ResetVectorAfterScope reset_vector(output_);
  base::span<CSSSelector> result =
      ConsumeComplexSelectorList(stream, CSSNestingType::kNone, result_flags);
  if (result.empty()) {
    return {};
  } else {
    CSSSelectorList* selector_list =
        CSSSelectorList::AdoptSelectorVector(result);
    return selector_list;
  }
}

CSSSelectorList* CSSSelectorParser::ConsumeForgivingNestedSelectorList(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  if (inside_compound_pseudo_) {
    return ConsumeForgivingCompoundSelectorList(stream, result_flags);
  }
  ResetVectorAfterScope reset_vector(output_);
  std::optional<base::span<CSSSelector>> forgiving_list =
      ConsumeForgivingComplexSelectorList(stream, CSSNestingType::kNone,
                                          result_flags);
  if (!forgiving_list.has_value()) {
    return nullptr;
  }
  return CSSSelectorList::AdoptSelectorVector(forgiving_list.value());
}

std::optional<base::span<CSSSelector>>
CSSSelectorParser::ConsumeForgivingComplexSelectorList(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    ResultFlags& result_flags) {
  if (in_supports_parsing_) {
    base::span<CSSSelector> selectors =
        ConsumeComplexSelectorList(stream, nesting_type, result_flags);
    if (selectors.empty()) {
      return std::nullopt;
    } else {
      return selectors;
    }
  }

  ResetVectorAfterScope reset_vector(output_);

  bool first_in_complex_selector_list = true;
  while (!stream.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenStream::State state = stream.Save();
    wtf_size_t subpos = output_.size();
    base::span<CSSSelector> selector = ConsumeComplexSelector(
        stream, nesting_type, first_in_complex_selector_list, result_flags);
    if (selector.empty() || failed_parsing_ ||
        !AtEndOfComplexSelector(stream)) {
      output_.resize(subpos);  // Drop what we parsed so far.
      stream.EnsureLookAhead();
      stream.Restore(state);
      AddPlaceholderSelectorIfNeeded(
          stream);  // Forwards until the end of the argument (i.e. to comma or
                    // EOB).
    }
    if (stream.Peek().GetType() != kCommaToken) {
      break;
    }
    stream.ConsumeIncludingWhitespace();
    first_in_complex_selector_list = false;
  }

  if (reset_vector.AddedElements().empty()) {
    //  Parsed nothing that was supported.
    return base::span<CSSSelector>();
  }

  return reset_vector.CommitAddedElements();
}

static CSSNestingType ConsumeUntilCommaAndFindNestingType(
    CSSParserTokenStream& stream) {
  CSSNestingType nesting_type = CSSNestingType::kNone;
  CSSParserToken previous_token(kIdentToken);

  while (!stream.AtEnd()) {
    const CSSParserToken& token = stream.Peek();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      CSSParserTokenStream::BlockGuard block(stream);
      while (!stream.AtEnd()) {
        nesting_type =
            std::max(nesting_type, ConsumeUntilCommaAndFindNestingType(stream));
        if (!stream.AtEnd()) {
          DCHECK_EQ(stream.Peek().GetType(), kCommaToken);
          stream.Consume();
        }
      }
      continue;
    }
    if (token.GetType() == kCommaToken) {
      // End of this argument.
      break;
    }
    if (token.GetType() == kDelimiterToken && token.Delimiter() == '&') {
      nesting_type = std::max(nesting_type, CSSNestingType::kNesting);
    }
    if (previous_token.GetType() == kColonToken &&
        token.GetType() == kIdentToken &&
        EqualIgnoringASCIICase(token.Value(), "scope")) {
      nesting_type = CSSNestingType::kScope;
    }

    previous_token = token;
    stream.Consume();
  }
  return nesting_type;
}

// If the argument was unparsable but contained a parent-referencing selector
// (& or :scope), we need to keep it so that we still consider the :is()
// as containing that selector; furthermore, we need to keep it on serialization
// so that a round-trip doesn't lose this information.
// We do not preserve comments fully.
//
// Note that this forwards the stream to the end of the argument (either to the
// next comma on the same nesting level, or the end of block).
void CSSSelectorParser::AddPlaceholderSelectorIfNeeded(
    CSSParserTokenStream& stream) {
  wtf_size_t start = stream.LookAheadOffset();
  CSSNestingType nesting_type = ConsumeUntilCommaAndFindNestingType(stream);
  stream.EnsureLookAhead();
  wtf_size_t end = stream.LookAheadOffset();

  if (nesting_type != CSSNestingType::kNone) {
    CSSSelector placeholder_selector;
    placeholder_selector.SetMatch(CSSSelector::kPseudoClass);
    placeholder_selector.SetUnparsedPlaceholder(
        nesting_type,
        stream.StringRangeAt(start, end - start).ToAtomicString());
    placeholder_selector.SetLastInComplexSelector(true);
    output_.push_back(placeholder_selector);
  }
}

CSSSelectorList* CSSSelectorParser::ConsumeForgivingCompoundSelectorList(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  if (in_supports_parsing_) {
    CSSSelectorList* selector_list =
        ConsumeCompoundSelectorList(stream, result_flags);
    if (!selector_list || !selector_list->IsValid()) {
      return nullptr;
    }
    return selector_list;
  }

  ResetVectorAfterScope reset_vector(output_);
  while (!stream.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    wtf_size_t subpos = output_.size();
    base::span<CSSSelector> selector =
        ConsumeCompoundSelector(stream, CSSNestingType::kNone, result_flags);
    stream.ConsumeWhitespace();
    if (selector.empty() || failed_parsing_ ||
        (!stream.AtEnd() && stream.Peek().GetType() != kCommaToken)) {
      output_.resize(subpos);  // Drop what we parsed so far.
      stream.SkipUntilPeekedTypeIs<kCommaToken>();
    } else {
      MarkAsEntireComplexSelector(selector);
    }
    if (!stream.AtEnd()) {
      stream.ConsumeIncludingWhitespace();
    }
  }

  if (reset_vector.AddedElements().empty()) {
    return CSSSelectorList::Empty();
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

CSSSelectorList* CSSSelectorParser::ConsumeForgivingRelativeSelectorList(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  if (in_supports_parsing_) {
    CSSSelectorList* selector_list =
        ConsumeRelativeSelectorList(stream, result_flags);
    if (!selector_list || !selector_list->IsValid()) {
      return nullptr;
    }
    return selector_list;
  }

  ResetVectorAfterScope reset_vector(output_);
  while (!stream.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenStream::BlockGuard guard(stream);
    wtf_size_t subpos = output_.size();
    base::span<CSSSelector> selector =
        ConsumeRelativeSelector(stream, result_flags);

    if (selector.empty() || failed_parsing_ ||
        (!stream.AtEnd() && stream.Peek().GetType() != kCommaToken)) {
      output_.resize(subpos);  // Drop what we parsed so far.
      stream.SkipUntilPeekedTypeIs<kCommaToken>();
    }
    if (!stream.AtEnd()) {
      stream.ConsumeIncludingWhitespace();
    }
  }

  // :has() is not allowed in the pseudos accepting only compound selectors, or
  // not allowed after pseudo-elements.
  // (e.g. '::slotted(:has(.a))', '::part(foo):has(:hover)')
  if (inside_compound_pseudo_ ||
      restricting_pseudo_element_ != CSSSelector::kPseudoUnknown ||
      reset_vector.AddedElements().empty()) {
    // TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
    // JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
    // Should return empty CSSSelectorList. (return CSSSelectorList::Empty())
    return nullptr;
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

CSSSelectorList* CSSSelectorParser::ConsumeRelativeSelectorList(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  ResetVectorAfterScope reset_vector(output_);
  if (ConsumeRelativeSelector(stream, result_flags).empty()) {
    return nullptr;
  }
  while (!stream.AtEnd() && stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeIncludingWhitespace();
    if (ConsumeRelativeSelector(stream, result_flags).empty()) {
      return nullptr;
    }
  }

  if (failed_parsing_) {
    return nullptr;
  }

  // :has() is not allowed in the pseudos accepting only compound selectors, or
  // not allowed after pseudo-elements.
  // (e.g. '::slotted(:has(.a))', '::part(foo):has(:hover)')
  if (inside_compound_pseudo_ ||
      restricting_pseudo_element_ != CSSSelector::kPseudoUnknown ||
      reset_vector.AddedElements().empty()) {
    return nullptr;
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

namespace {

enum CompoundSelectorFlags {
  kHasPseudoElementForRightmostCompound = 1 << 0,
};

unsigned ExtractCompoundFlags(const CSSSelector& simple_selector,
                              CSSParserMode parser_mode) {
  if (simple_selector.Match() != CSSSelector::kPseudoElement) {
    return 0;
  }
  // We don't restrict what follows custom ::-webkit-* pseudo-elements in UA
  // sheets. We currently use selectors in mediaControls.css like this:
  //
  // video::-webkit-media-text-track-region-container.scrolling
  if (parser_mode == kUASheetMode &&
      simple_selector.GetPseudoType() ==
          CSSSelector::kPseudoWebKitCustomElement) {
    return 0;
  }
  return kHasPseudoElementForRightmostCompound;
}

unsigned ExtractCompoundFlags(const base::span<CSSSelector> compound_selector,
                              CSSParserMode parser_mode) {
  unsigned compound_flags = 0;
  for (const CSSSelector& simple : compound_selector) {
    if (compound_flags) {
      break;
    }
    compound_flags |= ExtractCompoundFlags(simple, parser_mode);
  }
  return compound_flags;
}

}  // namespace

base::span<CSSSelector> CSSSelectorParser::ConsumeRelativeSelector(
    CSSParserTokenStream& stream,
    ResultFlags& result_flags) {
  ResetVectorAfterScope reset_vector(output_);

  CSSSelector selector;
  selector.SetMatch(CSSSelector::kPseudoClass);
  selector.UpdatePseudoType(AtomicString("-internal-relative-anchor"),
                            *context_, false /*has_arguments*/,
                            context_->Mode());
  DCHECK_EQ(selector.GetPseudoType(), CSSSelector::kPseudoRelativeAnchor);
  output_.push_back(selector);

  CSSSelector::RelationType combinator =
      ConvertRelationToRelative(ConsumeCombinator(stream));
  unsigned previous_compound_flags = 0;

  if (!ConsumePartialComplexSelector(stream, combinator,
                                     previous_compound_flags,
                                     CSSNestingType::kNone, result_flags)) {
    return {};
  }

  // See ConsumeComplexSelector().
  std::ranges::reverse(reset_vector.AddedElements());

  MarkAsEntireComplexSelector(reset_vector.AddedElements());
  return reset_vector.CommitAddedElements();
}

// This acts like CSSSelector::GetNestingType, except across a whole
// selector list.
//
// A return value of CSSNestingType::kNesting means that the list
// "contains the nesting selector".
// https://drafts.csswg.org/css-nesting-1/#contain-the-nesting-selector
//
// A return value of CSSNestingType::kScope means that the list
// contains the :scope selector.
static CSSNestingType GetNestingTypeForSelectorList(
    const CSSSelector* selector) {
  if (selector == nullptr) {
    return CSSNestingType::kNone;
  }
  CSSNestingType nesting_type = CSSNestingType::kNone;
  for (;;) {  // Termination condition within loop.
    nesting_type = std::max(nesting_type, selector->GetNestingType());

    auto* list = selector->SelectorList();
    if (list != nullptr) {
      nesting_type =
          std::max(nesting_type, GetNestingTypeForSelectorList(list->First()));
    }
    if (selector->IsLastInSelectorList() ||
        nesting_type == CSSNestingType::kNesting) {
      break;
    }

    // SAFETY: SelectorList uses iterator pattern,
    // so it is safe to increment the pointer as long
    // as we check the item is not nullptr before dereferencing.
    UNSAFE_BUFFERS(++selector);
  }
  return nesting_type;
}

// This acts like CSSSelector::GetNestingType, except across a whole
// selector list.
//
// A return value of CSSNestingType::kNesting means that the list
// "contains the nesting selector".
// https://drafts.csswg.org/css-nesting-1/#contain-the-nesting-selector
//
// A return value of CSSNestingType::kScope means that the list
// contains the :scope selector.
static CSSNestingType GetNestingTypeForSelectorList(
    base::span<const CSSSelector> selector) {
  if (selector.empty()) {
    return CSSNestingType::kNone;
  }
  CSSNestingType nesting_type = CSSNestingType::kNone;
  for (const CSSSelector& curr : selector) {
    nesting_type = std::max(nesting_type, curr.GetNestingType());

    auto* list = curr.SelectorList();
    if (list != nullptr) {
      nesting_type =
          std::max(nesting_type, GetNestingTypeForSelectorList(list->First()));
    }
    if (curr.IsLastInSelectorList() ||
        nesting_type == CSSNestingType::kNesting) {
      break;
    }
  }
  return nesting_type;
}

// https://drafts.csswg.org/selectors/#relative-selector-anchor-elements
static CSSSelector CreateImplicitAnchor(
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting) {
  DCHECK(nesting_type == CSSNestingType::kNesting ||
         nesting_type == CSSNestingType::kScope);
  CSSSelector selector =
      (nesting_type == CSSNestingType::kNesting)
          ? CSSSelector(parent_rule_for_nesting, /*is_implicit=*/true)
          : CSSSelector(AtomicString("scope"), /*is_implicit=*/true);
  selector.SetScopeContaining(true);
  return selector;
}

static std::optional<CSSSelector> MaybeCreateImplicitDescendantAnchor(
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    base::span<const CSSSelector> selector) {
  switch (nesting_type) {
    case CSSNestingType::kNone:
      break;
    case CSSNestingType::kScope:
    case CSSNestingType::kNesting:
      static_assert(CSSNestingType::kNone < CSSNestingType::kScope);
      static_assert(CSSNestingType::kScope < CSSNestingType::kNesting);
      // For kNesting, we should only produce an implied descendant combinator
      // if the selector list is not nest-containing.
      //
      // For kScope, we should should only produce an implied descendant
      // combinator if the selector list is not :scope-containing. Note however
      // that selectors which are nest-containing are also treated as
      // :scope-containing.
      if (GetNestingTypeForSelectorList(selector) < nesting_type) {
        return CreateImplicitAnchor(nesting_type, parent_rule_for_nesting);
      }
      break;
    case CSSNestingType::kFunction:
      NOTREACHED();
  }
  return std::nullopt;
}

// A nested rule that starts with a combinator; very similar to
// ConsumeRelativeSelector() (but we don't use the kRelative* relations,
// as they have different matching semantics). There's an implicit anchor
// compound in front, which for CSSNestingType::kNesting is the nesting
// selector (&) and for CSSNestingType::kScope is the :scope pseudo-class.
// E.g. given CSSNestingType::kNesting, “> .a” is parsed as “& > .a” ().
base::span<CSSSelector> CSSSelectorParser::ConsumeNestedRelativeSelector(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    ResultFlags& result_flags) {
  DCHECK_NE(nesting_type, CSSNestingType::kNone);

  ResetVectorAfterScope reset_vector(output_);
  output_.push_back(
      CreateImplicitAnchor(nesting_type, parent_rule_for_nesting_));
  result_flags |= kContainsScopeOrParent;
  CSSSelector::RelationType combinator = ConsumeCombinator(stream);
  unsigned previous_compound_flags = 0;
  if (!ConsumePartialComplexSelector(stream, combinator,
                                     previous_compound_flags, nesting_type,
                                     result_flags)) {
    return {};
  }

  std::ranges::reverse(reset_vector.AddedElements());

  MarkAsEntireComplexSelector(reset_vector.AddedElements());
  return reset_vector.CommitAddedElements();
}

base::span<CSSSelector> CSSSelectorParser::ConsumeComplexSelector(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    bool first_in_complex_selector_list,
    ResultFlags& result_flags) {
  if (nesting_type != CSSNestingType::kNone && PeekIsCombinator(stream)) {
    // Nested selectors that start with a combinator are to be
    // interpreted as relative selectors (with the anchor being
    // the parent selector, i.e., &).
    return ConsumeNestedRelativeSelector(stream, nesting_type, result_flags);
  }

  ResetVectorAfterScope reset_vector(output_);
  base::span<CSSSelector> compound_selector =
      ConsumeCompoundSelector(stream, nesting_type, result_flags);
  if (compound_selector.empty()) {
    return {};
  }

  // Reverse the compound selector, so that it comes out properly
  // after we reverse everything below.
  std::ranges::reverse(compound_selector);

  if (CSSSelector::RelationType combinator = ConsumeCombinator(stream)) {
    result_flags |= kContainsComplexSelector;
    unsigned previous_compound_flags =
        ExtractCompoundFlags(compound_selector, context_->Mode());
    if (!ConsumePartialComplexSelector(stream, combinator,
                                       previous_compound_flags, nesting_type,
                                       result_flags)) {
      return {};
    }
  }

  // Complex selectors (i.e., groups of compound selectors) are stored
  // right-to-left, ie., the opposite direction of what we parse them. However,
  // within each compound selector, the simple selectors are stored
  // left-to-right. The simplest way of doing this in-place is to reverse each
  // compound selector after we've parsed it (which we do above), and then
  // reverse the entire list in the end. So if the CSS text says:
  //
  //   .a.b.c .d.e.f .g.h
  //
  // we first parse and reverse each compound selector:
  //
  //   .c.b.a .f.e.d .h.g
  //
  // and then reverse the entire list, giving the desired in-memory layout:
  //
  //   .g.h .d.e.f .a.b.c
  //
  // The boundaries between the compound selectors are implicit; they are given
  // by having a Relation() not equal to kSubSelector, so they follow
  // automatically when we do the reversal.
  std::ranges::reverse(reset_vector.AddedElements());

  if (nesting_type != CSSNestingType::kNone) {
    // In nested top-level rules, if we do not have a & anywhere in the list,
    // we are a relative selector (with & as the anchor), and we must prepend
    // (or append, since we're storing reversed) an implicit & using
    // a descendant combinator.
    //
    // We need to temporarily mark the end of the selector list, for the benefit
    // of GetNestingTypeForSelectorList().
    wtf_size_t last_index = output_.size() - 1;
    output_[last_index].SetLastInSelectorList(true);
    if (std::optional<CSSSelector> anchor = MaybeCreateImplicitDescendantAnchor(
            nesting_type, parent_rule_for_nesting_,
            reset_vector.AddedElements())) {
      output_.back().SetRelation(CSSSelector::kDescendant);
      output_.push_back(anchor.value());
      result_flags |= kContainsScopeOrParent;
    }

    output_[last_index].SetLastInSelectorList(false);
  }

  MarkAsEntireComplexSelector(reset_vector.AddedElements());

  return reset_vector.CommitAddedElements();
}

bool CSSSelectorParser::ConsumePartialComplexSelector(
    CSSParserTokenStream& stream,
    CSSSelector::RelationType& combinator,
    unsigned previous_compound_flags,
    CSSNestingType nesting_type,
    ResultFlags& result_flags) {
  do {
    base::span<CSSSelector> compound_selector =
        ConsumeCompoundSelector(stream, nesting_type, result_flags);
    if (compound_selector.empty()) {
      // No more selectors. If we ended with some explicit combinator
      // (e.g. “a >” and then nothing), that's a parse error.
      // But if not, we're simply done and return everything
      // we've parsed so far.
      return combinator == CSSSelector::kDescendant;
    }
    compound_selector.back().SetRelation(combinator);

    // See ConsumeComplexSelector().
    std::ranges::reverse(compound_selector);

    if (previous_compound_flags & kHasPseudoElementForRightmostCompound) {
      // If we've already seen a compound that needs to be rightmost, and still
      // get more, that's a parse error.
      return false;
    }
    previous_compound_flags =
        ExtractCompoundFlags(compound_selector, context_->Mode());
  } while ((combinator = ConsumeCombinator(stream)));

  return true;
}

// static
CSSSelector::PseudoType CSSSelectorParser::ParsePseudoType(
    const AtomicString& name,
    bool has_arguments,
    const Document* document) {
  CSSSelector::PseudoType pseudo_type =
      CSSSelector::NameToPseudoType(name, has_arguments, document);

  if (pseudo_type != CSSSelector::PseudoType::kPseudoUnknown) {
    return pseudo_type;
  }

  if (name.StartsWith("-webkit-")) {
    return CSSSelector::PseudoType::kPseudoWebKitCustomElement;
  }
  if (name.StartsWith("-internal-")) {
    return CSSSelector::PseudoType::kPseudoBlinkInternalElement;
  }

  return CSSSelector::PseudoType::kPseudoUnknown;
}

// static
PseudoId CSSSelectorParser::ParsePseudoElement(const String& selector_string,
                                               const Node* parent,
                                               AtomicString& argument) {
  // For old pseudos (before, after, first-letter, first-line), we
  // allow the legacy behavior of single-colon / no-colon.
  {
    CSSParserTokenStream stream(selector_string);
    stream.EnsureLookAhead();
    int num_colons = 0;
    if (stream.Peek().GetType() == kColonToken) {
      stream.Consume();
      ++num_colons;
    }
    if (stream.Peek().GetType() == kColonToken) {
      stream.Consume();
      ++num_colons;
    }

    CSSParserToken selector_name_token = stream.Peek();
    if (selector_name_token.GetType() == kIdentToken) {
      stream.Consume();
      if (!selector_name_token.Value().ContainsOnlyASCIIOrEmpty()) {
        return kPseudoIdInvalid;
      }
      if (stream.Peek().GetType() != kEOFToken) {
        return kPseudoIdInvalid;
      }

      CSSSelector::PseudoType pseudo_type = ParsePseudoType(
          selector_name_token.Value().ToAtomicString(),
          /*has_arguments=*/false, parent ? &parent->GetDocument() : nullptr);

      PseudoId pseudo_id = CSSSelector::GetPseudoId(pseudo_type);
      if (pseudo_id == kPseudoIdBefore || pseudo_id == kPseudoIdAfter ||
          pseudo_id == kPseudoIdFirstLetter ||
          pseudo_id == kPseudoIdFirstLine) {
        return pseudo_id;
      }

      // For ::-webkit-* and ::-internal-* pseudo-elements, act like there's
      // no pseudo-element provided and (at least for getComputedStyle, our
      // most significant caller) use the element style instead.
      // TODO(https://crbug.com/363015176): We should either do something
      // correct or treat them as unsupported.
      if ((pseudo_type == CSSSelector::PseudoType::kPseudoWebKitCustomElement ||
           pseudo_type ==
               CSSSelector::PseudoType::kPseudoBlinkInternalElement) &&
          num_colons == 2) {
        return kPseudoIdNone;
      }
    }

    if (num_colons != 2) {
      return num_colons == 1 ? kPseudoIdInvalid : kPseudoIdNone;
    }
  }

  // Otherwise, we use the standard pseudo-selector parser.
  // A restart is OK here, since this function is called only from
  // getComputedStyle() and similar, not the main parsing path.
  HeapVector<CSSSelector> arena;
  CSSSelectorParser parser(
      StrictCSSParserContext(SecureContextMode::kInsecureContext),
      /*parent_rule_for_nesting=*/nullptr,
      /*semicolon_aborts_nested_selector=*/false,
      /*style_sheet=*/nullptr, arena);

  ResetVectorAfterScope reset_vector(parser.output_);
  CSSParserTokenStream stream(selector_string);
  ResultFlags result_flags = 0;
  if (!parser.ConsumePseudo(stream, result_flags)) {
    return kPseudoIdInvalid;
  }

  auto selector = reset_vector.AddedElements();
  if (selector.size() != 1 || !stream.AtEnd()) {
    return kPseudoIdInvalid;
  }

  const CSSSelector& result = selector[0];
  if (!result.MatchesPseudoElement()) {
    return kPseudoIdInvalid;
  }

  PseudoId pseudo_id = result.GetPseudoId(result.GetPseudoType());
  if (!PseudoElement::IsWebExposed(pseudo_id, parent)) {
    return kPseudoIdInvalid;
  }

  switch (pseudo_id) {
    case kPseudoIdHighlight: {
      argument = result.Argument();
      return pseudo_id;
    }

    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionGroupChildren:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew: {
      if (result.IdentList().size() != 1 ||
          result.IdentList()[0] == CSSSelector::UniversalSelectorAtom()) {
        return kPseudoIdInvalid;
      }
      argument = result.IdentList()[0];
      return pseudo_id;
    }

    default:
      return pseudo_id;
  }
}

namespace {

bool IsScrollbarPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoActive:
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoCornerPresent:
    case CSSSelector::kPseudoWindowInactive:
      return true;
    default:
      return false;
  }
}

bool IsUserActionPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoFocus:
    case CSSSelector::kPseudoFocusVisible:
    case CSSSelector::kPseudoFocusWithin:
    case CSSSelector::kPseudoActive:
      return true;
    default:
      return false;
  }
}

bool IsUserActionPseudoClassAllowedAfterPseudoElement(
    CSSSelector::PseudoType pseudo_class,
    CSSSelector::PseudoType compound_pseudo_element) {
  if (!IsUserActionPseudoClass(pseudo_class)) {
    return false;
  }
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoScrollButton:
    case CSSSelector::kPseudoScrollMarker:
      return true;
    default:
      // TODO(crbug.com/40824273): User action pseudos should be allowed more
      // generally after pseudo-elements.
      return false;
  }
}

bool IsPseudoClassValidAfterPseudoElement(
    CSSSelector::PseudoType pseudo_class,
    CSSSelector::PseudoType compound_pseudo_element) {
  if (IsUserActionPseudoClassAllowedAfterPseudoElement(
          pseudo_class, compound_pseudo_element)) {
    return true;
  }
  // NOTE: pseudo-class rules for ::part() and element-backed pseudo-elements
  // do not need to be handled here; they should be handled in
  // CSSSelector::IsAllowedAfterPart() instead.
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoResizer:
    case CSSSelector::kPseudoScrollbar:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece:
      return IsScrollbarPseudoClass(pseudo_class);
    case CSSSelector::kPseudoSelection:
      return pseudo_class == CSSSelector::kPseudoWindowInactive;
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
    case CSSSelector::kPseudoFileSelectorButton:
      return IsUserActionPseudoClass(pseudo_class);
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionGroupChildren:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoViewTransitionNew:
      return pseudo_class == CSSSelector::kPseudoOnlyChild;
    case CSSSelector::kPseudoSearchText:
      return pseudo_class == CSSSelector::kPseudoCurrent;
    case CSSSelector::kPseudoScrollMarkerGroup:
      return pseudo_class == CSSSelector::kPseudoFocusWithin;
    case CSSSelector::kPseudoScrollMarker:
      return pseudo_class == CSSSelector::kPseudoTargetCurrent ||
             pseudo_class == CSSSelector::kPseudoTargetBefore ||
             pseudo_class == CSSSelector::kPseudoTargetAfter;
    case CSSSelector::kPseudoScrollButton:
      return pseudo_class == CSSSelector::kPseudoDisabled ||
             pseudo_class == CSSSelector::kPseudoEnabled;
    default:
      return false;
  }
}

bool IsSimpleSelectorValidAfterPseudoElement(
    const CSSSelector& simple_selector,
    CSSSelector::PseudoType compound_pseudo_element) {
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoColumn:
      return simple_selector.GetPseudoType() ==
             CSSSelector::kPseudoScrollMarker;
    case CSSSelector::kPseudoUnknown:
      return true;
    case CSSSelector::kPseudoAfter:
    case CSSSelector::kPseudoBefore:
      if (simple_selector.GetPseudoType() == CSSSelector::kPseudoMarker &&
          RuntimeEnabledFeatures::CSSMarkerNestedPseudoElementEnabled()) {
        return true;
      }
      break;
    case CSSSelector::kPseudoSlotted:
      return simple_selector.IsTreeAbidingPseudoElement();
    default:
      break;
  }
  if ((compound_pseudo_element == CSSSelector::kPseudoPart ||
       CSSSelector::IsElementBackedPseudoElement(compound_pseudo_element)) &&
      simple_selector.IsAllowedAfterPart()) {
    return true;
  }
  if (simple_selector.Match() != CSSSelector::kPseudoClass) {
    return false;
  }
  CSSSelector::PseudoType pseudo = simple_selector.GetPseudoType();
  switch (pseudo) {
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoNot:
      // These pseudo-classes are themselves always valid.
      // CSSSelectorParser::restricting_pseudo_element_ ensures that invalid
      // nested selectors will be dropped if they are invalid according to
      // this function.
      return true;
    default:
      break;
  }
  return IsPseudoClassValidAfterPseudoElement(pseudo, compound_pseudo_element);
}

bool IsPseudoClassValidWithinHasArgument(CSSSelector& selector) {
  DCHECK_EQ(selector.Match(), CSSSelector::kPseudoClass);
  switch (selector.GetPseudoType()) {
    // Limited nested :has() to avoid increasing :has() invalidation complexity.
    case CSSSelector::kPseudoHas:
      return false;
    default:
      return true;
  }
}

}  // namespace

base::span<CSSSelector> CSSSelectorParser::ConsumeCompoundSelector(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    ResultFlags& result_flags) {
  ResetVectorAfterScope reset_vector(output_);
  wtf_size_t start_pos = output_.size();
  base::AutoReset<CSSSelector::PseudoType> reset_restricting(
      &restricting_pseudo_element_, restricting_pseudo_element_);
  base::AutoReset<bool> reset_found_host_in_compound(&found_host_in_compound_,
                                                     false);

  // See if the compound selector starts with a tag name, universal selector
  // or the likes (these can only be at the beginning). Note that we don't
  // add this to output_ yet, because there are situations where it should
  // be ignored (like if we have a universal selector and don't need it;
  // e.g. *:hover is the same as :hover). Thus, we just keep its data around
  // and prepend it if needed.
  //
  // TODO(sesse): In 99% of cases, we should add this, so the prepending logic
  // gets very complex with having to deal with both the explicit and the
  // implicit case. Consider just inserting it, and then removing it
  // afterwards if we really don't need it.
  AtomicString namespace_prefix;
  AtomicString element_name;
  const bool has_q_name = ConsumeName(stream, element_name, namespace_prefix);
  if (context_->IsHTMLDocument()) {
    element_name = element_name.LowerASCII();
  }

  // A tag name is not valid following a pseudo-element. This can happen for
  // e.g. :::part(x):is(div).
  if (restricting_pseudo_element_ != CSSSelector::kPseudoUnknown &&
      has_q_name) {
    failed_parsing_ = true;
    return {};  // Failure.
  }

  std::vector<size_t> has_pseudo_index_in_compound;
  // Consume all the simple selectors that are not tag names.
  while (ConsumeSimpleSelector(stream, result_flags)) {
    const CSSSelector& simple_selector = output_.back();
    if (simple_selector.Match() == CSSSelector::kPseudoElement) {
      restricting_pseudo_element_ = simple_selector.GetPseudoType();
    }
    if (simple_selector.GetPseudoType() == CSSSelector::kPseudoHas) {
      has_pseudo_index_in_compound.push_back(output_.size() - 1);
    }
    output_.back().SetRelation(CSSSelector::kSubSelector);
  }
  if (found_host_in_compound_) {
    for (size_t has_pseudo_index : has_pseudo_index_in_compound) {
      DCHECK_LT(has_pseudo_index, output_.size());
      CSSSelector* has_pseudo = &output_[has_pseudo_index];
      DCHECK_EQ(has_pseudo->GetPseudoType(), CSSSelector::kPseudoHas);
      has_pseudo->SetHasArgumentMatchInShadowTree();
    }
  }

  // While inside a nested selector like :is(), the default namespace shall
  // be ignored when [1]:
  //
  // - The compound selector represents the subject [2], and
  // - The compound selector does not contain a type/universal selector.
  //
  // [1] https://drafts.csswg.org/selectors/#matches
  // [2] https://drafts.csswg.org/selectors/#selector-subject
  base::AutoReset<bool> ignore_namespace(
      &ignore_default_namespace_,
      ignore_default_namespace_ || (resist_default_namespace_ && !has_q_name &&
                                    AtEndIgnoringWhitespace(stream)));

  if (reset_vector.AddedElements().empty()) {
    // No simple selectors except for the tag name.
    // TODO(sesse): Does this share too much code with
    // PrependTypeSelectorIfNeeded()?
    if (!has_q_name) {
      // No tag name either, so we fail parsing of this selector.
      return {};
    }
    DCHECK(has_q_name);
    AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
    if (namespace_uri.IsNull()) {
      context_->Count(WebFeature::kCSSUnknownNamespacePrefixInSelector);
      failed_parsing_ = true;
      return {};
    }
    if (namespace_uri == DefaultNamespace()) {
      namespace_prefix = g_null_atom;
    }
    output_.push_back(CSSSelector(
        QualifiedName(namespace_prefix, element_name, namespace_uri)));
    return reset_vector.CommitAddedElements();
  }

  // Prepend a tag selector if we have one, either explicitly or implicitly.
  // One could be added implicitly e.g. if we are in a non-default namespace
  // and have no tag selector already, we may need to convert .foo to
  // (ns|*).foo, with an implicit universal selector prepended before .foo.
  // The explicit case is when we simply have a tag; e.g. if someone wrote
  // div.foo.bar, we've added .foo.bar earlier and are prepending div now.
  //
  // TODO(futhark@chromium.org): Prepending a type selector to the compound is
  // unnecessary if this compound is an argument to a pseudo selector like
  // :not(), since a type selector will be prepended at the top level of the
  // selector if necessary. We need to propagate that context information here
  // to tell if we are at the top level.
  PrependTypeSelectorIfNeeded(namespace_prefix, has_q_name, element_name,
                              start_pos);

  // The relationship between all of these are that they are sub-selectors.
  for (CSSSelector& selector : reset_vector.AddedElements().first(
           reset_vector.AddedElements().size() - 1)) {
    selector.SetRelation(CSSSelector::kSubSelector);
  }

  SplitCompoundAtImplicitCombinator(reset_vector.AddedElements());
  return reset_vector.CommitAddedElements();
}

bool CSSSelectorParser::ConsumeSimpleSelector(CSSParserTokenStream& stream,
                                              ResultFlags& result_flags) {
  ResultFlags local_result_flags = 0;
  const CSSParserToken& token = stream.Peek();
  bool ok;
  if (token.GetType() == kHashToken) {
    ok = ConsumeId(stream);
  } else if (token.GetType() == kDelimiterToken && token.Delimiter() == '.') {
    ok = ConsumeClass(stream);
  } else if (token.GetType() == kLeftBracketToken) {
    ok = ConsumeAttribute(stream);
  } else if (token.GetType() == kColonToken) {
    ok = ConsumePseudo(stream, local_result_flags);
    if (ok) {
      local_result_flags |= kContainsPseudo;
    }
  } else if (token.GetType() == kDelimiterToken && token.Delimiter() == '&') {
    ok = ConsumeNestingParent(stream, local_result_flags);
  } else {
    return false;
  }
  // TODO(futhark@chromium.org): crbug.com/578131
  // The UASheetMode check is a work-around to allow this selector in
  // mediaControls(New).css:
  // video::-webkit-media-text-track-region-container.scrolling
  if (!ok || (context_->Mode() != kUASheetMode &&
              !IsSimpleSelectorValidAfterPseudoElement(
                  output_.back(), restricting_pseudo_element_))) {
    failed_parsing_ = true;
    return false;
  }
  if (local_result_flags & kContainsScopeOrParent) {
    output_.back().SetScopeContaining(true);
  }
  result_flags |= local_result_flags;
  return true;
}

bool CSSSelectorParser::ConsumeName(CSSParserTokenStream& stream,
                                    AtomicString& name,
                                    AtomicString& namespace_prefix) {
  name = g_null_atom;
  namespace_prefix = g_null_atom;

  const CSSParserToken& first_token = stream.Peek();
  if (first_token.GetType() == kIdentToken) {
    name = first_token.Value().ToAtomicString();
    stream.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '*') {
    name = CSSSelector::UniversalSelectorAtom();
    stream.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '|') {
    // This is an empty namespace, which'll get assigned this value below
    name = g_empty_atom;
  } else {
    return false;
  }

  if (stream.Peek().GetType() != kDelimiterToken ||
      stream.Peek().Delimiter() != '|') {
    return true;
  }

  CSSParserSavePoint savepoint(stream);
  stream.Consume();

  namespace_prefix =
      name == CSSSelector::UniversalSelectorAtom() ? g_star_atom : name;
  if (stream.Peek().GetType() == kIdentToken) {
    name = stream.Consume().Value().ToAtomicString();
  } else if (stream.Peek().GetType() == kDelimiterToken &&
             stream.Peek().Delimiter() == '*') {
    stream.Consume();
    name = CSSSelector::UniversalSelectorAtom();
  } else {
    name = g_null_atom;
    namespace_prefix = g_null_atom;
    return false;
  }

  savepoint.Release();
  return true;
}

bool CSSSelectorParser::ConsumeId(CSSParserTokenStream& stream) {
  DCHECK_EQ(stream.Peek().GetType(), kHashToken);
  if (stream.Peek().GetHashTokenType() != kHashTokenId) {
    return false;
  }
  CSSSelector selector;
  selector.SetMatch(CSSSelector::kId);
  AtomicString value = stream.Consume().Value().ToAtomicString();
  selector.SetValue(value, IsQuirksModeBehavior(context_->Mode()));
  output_.push_back(std::move(selector));
  return true;
}

bool CSSSelectorParser::ConsumeClass(CSSParserTokenStream& stream) {
  DCHECK_EQ(stream.Peek().GetType(), kDelimiterToken);
  DCHECK_EQ(stream.Peek().Delimiter(), '.');
  stream.Consume();
  if (stream.Peek().GetType() != kIdentToken) {
    return false;
  }
  CSSSelector selector;
  selector.SetMatch(CSSSelector::kClass);
  AtomicString value = stream.Consume().Value().ToAtomicString();
  selector.SetValue(value, IsQuirksModeBehavior(context_->Mode()));
  output_.push_back(std::move(selector));
  return true;
}

bool CSSSelectorParser::ConsumeAttribute(CSSParserTokenStream& stream) {
  DCHECK_EQ(stream.Peek().GetType(), kLeftBracketToken);
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();

  AtomicString namespace_prefix;
  AtomicString attribute_name;
  if (!ConsumeName(stream, attribute_name, namespace_prefix)) {
    return false;
  }
  if (attribute_name == CSSSelector::UniversalSelectorAtom()) {
    return false;
  }
  stream.ConsumeWhitespace();

  if (context_->IsHTMLDocument()) {
    attribute_name = attribute_name.LowerASCII();
  }

  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull()) {
    return false;
  }

  QualifiedName qualified_name =
      namespace_prefix.IsNull()
          ? QualifiedName(attribute_name)
          : QualifiedName(namespace_prefix, attribute_name, namespace_uri);

  if (stream.AtEnd()) {
    CSSSelector selector(CSSSelector::kAttributeSet, qualified_name,
                         CSSSelector::AttributeMatchType::kCaseSensitive);
    output_.push_back(std::move(selector));
    return true;
  }

  CSSSelector::MatchType match_type = ConsumeAttributeMatch(stream);

  CSSParserToken attribute_value = stream.Peek();
  if (attribute_value.GetType() != kIdentToken &&
      attribute_value.GetType() != kStringToken) {
    return false;
  }
  stream.ConsumeIncludingWhitespace();
  CSSSelector::AttributeMatchType case_sensitivity =
      ConsumeAttributeFlags(stream);
  if (!stream.AtEnd()) {
    return false;
  }

  CSSSelector selector(match_type, qualified_name, case_sensitivity,
                       attribute_value.Value().ToAtomicString());
  output_.push_back(std::move(selector));
  return true;
}

bool CSSSelectorParser::ConsumePseudo(CSSParserTokenStream& stream,
                                      ResultFlags& result_flags) {
  DCHECK_EQ(stream.Peek().GetType(), kColonToken);
  stream.Consume();

  int colons = 1;
  if (stream.Peek().GetType() == kColonToken) {
    stream.Consume();
    colons++;
  }

  const CSSParserToken& token = stream.Peek();
  if (token.GetType() != kIdentToken && token.GetType() != kFunctionToken) {
    return false;
  }

  CSSSelector selector;
  selector.SetMatch(colons == 1 ? CSSSelector::kPseudoClass
                                : CSSSelector::kPseudoElement);

  bool has_arguments = token.GetType() == kFunctionToken;
  selector.UpdatePseudoType(token.Value().ToAtomicString(), *context_,
                            has_arguments, context_->Mode());

  if (selector.Match() == CSSSelector::kPseudoElement) {
    switch (selector.GetPseudoType()) {
      case CSSSelector::kPseudoBefore:
      case CSSSelector::kPseudoAfter:
        context_->Count(WebFeature::kHasBeforeOrAfterPseudoElement);
        break;
      case CSSSelector::kPseudoMarker:
        if (context_->Mode() != kUASheetMode) {
          context_->Count(WebFeature::kHasMarkerPseudoElement);
        }
        break;
      case CSSSelector::kPseudoSpellingError:
      case CSSSelector::kPseudoGrammarError:
        if (context_->Mode() != kUASheetMode) {
          context_->Count(WebFeature::kHasSpellingOrGrammarErrorPseudoElement);
        }
        break;
      default:
        break;
    }
  }

  if (selector.Match() == CSSSelector::kPseudoElement &&
      disallow_pseudo_elements_) {
    return false;
  }

  if (is_inside_has_argument_) {
    DCHECK(disallow_pseudo_elements_);
    if (!IsPseudoClassValidWithinHasArgument(selector)) {
      return false;
    }
  }

  if (token.GetType() == kIdentToken) {
    stream.Consume();
    if (selector.GetPseudoType() == CSSSelector::kPseudoUnknown) {
      return false;
    }
    if (selector.GetPseudoType() == CSSSelector::kPseudoHost) {
      found_host_in_compound_ = true;
    }
    if (selector.GetPseudoType() == CSSSelector::kPseudoScope) {
      result_flags |= kContainsScopeOrParent;
    }
    output_.push_back(std::move(selector));
    return true;
  }

  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  if (selector.GetPseudoType() == CSSSelector::kPseudoUnknown) {
    return false;
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoIs: {
      std::optional<DisallowPseudoElementsScope> disallow_pseudo_elements;
      if (!RuntimeEnabledFeatures::CSSLogicalCombinationPseudoEnabled()) {
        disallow_pseudo_elements.emplace(this);
      }
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      CSSSelectorList* selector_list =
          ConsumeForgivingNestedSelectorList(stream, result_flags);
      if (!selector_list || !stream.AtEnd()) {
        return false;
      }
      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoWhere: {
      std::optional<DisallowPseudoElementsScope> disallow_pseudo_elements;
      if (!RuntimeEnabledFeatures::CSSLogicalCombinationPseudoEnabled()) {
        disallow_pseudo_elements.emplace(this);
      }
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      CSSSelectorList* selector_list =
          ConsumeForgivingNestedSelectorList(stream, result_flags);
      if (!selector_list || !stream.AtEnd()) {
        return false;
      }
      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
      found_host_in_compound_ = true;
      [[fallthrough]];
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoCue: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> inside_compound(&inside_compound_pseudo_, true);
      base::AutoReset<bool> ignore_namespace(
          &ignore_default_namespace_,
          ignore_default_namespace_ ||
              selector.GetPseudoType() == CSSSelector::kPseudoCue);

      CSSSelectorList* selector_list =
          ConsumeCompoundSelectorList(stream, result_flags);
      if (!selector_list || !selector_list->IsValid() || !stream.AtEnd()) {
        return false;
      }

      if (!selector_list->IsSingleComplexSelector()) {
        if (selector.GetPseudoType() == CSSSelector::kPseudoHost) {
          return false;
        }
        if (selector.GetPseudoType() == CSSSelector::kPseudoHostContext) {
          return false;
        }
      }

      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoHas: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);

      base::AutoReset<bool> is_inside_has_argument(&is_inside_has_argument_,
                                                   true);
      ResultFlags local_result_flags = 0;
      CSSSelectorList* selector_list;
      selector_list = ConsumeRelativeSelectorList(stream, local_result_flags);
      if (!selector_list || !selector_list->IsValid() || !stream.AtEnd()) {
        return false;
      }
      selector.SetSelectorList(selector_list);
      if (local_result_flags & kContainsPseudo) {
        selector.SetContainsPseudoInsideHasPseudoClass();
      }
      if (local_result_flags & kContainsComplexSelector) {
        selector.SetContainsComplexLogicalCombinationsInsideHasPseudoClass();
      }
      output_.push_back(std::move(selector));
      result_flags |= local_result_flags;
      return true;
    }
    case CSSSelector::kPseudoNot: {
      std::optional<DisallowPseudoElementsScope> disallow_pseudo_elements;
      if (!RuntimeEnabledFeatures::CSSLogicalCombinationPseudoEnabled()) {
        disallow_pseudo_elements.emplace(this);
      }
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      CSSSelectorList* selector_list =
          ConsumeNestedSelectorList(stream, result_flags);
      if (!selector_list || !selector_list->IsValid() || !stream.AtEnd()) {
        return false;
      }

      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoPicker:
    case CSSSelector::kPseudoDir:
    case CSSSelector::kPseudoState: {
      const CSSParserToken& ident = stream.Peek();
      if (ident.GetType() != kIdentToken) {
        return false;
      }
      selector.SetArgument(ident.Value().ToAtomicString());
      stream.ConsumeIncludingWhitespace();
      if (!stream.AtEnd()) {
        return false;
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoPart: {
      Vector<AtomicString> parts;
      do {
        const CSSParserToken& ident = stream.Peek();
        if (ident.GetType() != kIdentToken) {
          return false;
        }
        parts.push_back(ident.Value().ToAtomicString());
        stream.ConsumeIncludingWhitespace();
      } while (!stream.AtEnd());
      selector.SetIdentList(std::make_unique<Vector<AtomicString>>(parts));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoActiveViewTransitionType: {
      Vector<AtomicString> types;
      for (;;) {
        const CSSParserToken& ident = stream.Peek();
        if (ident.GetType() != kIdentToken) {
          return false;
        }
        types.push_back(ident.Value().ToAtomicString());
        stream.ConsumeIncludingWhitespace();

        if (stream.AtEnd()) {
          break;
        }

        const CSSParserToken& comma = stream.Peek();
        if (comma.GetType() != kCommaToken) {
          return false;
        }
        stream.ConsumeIncludingWhitespace();
        if (stream.AtEnd()) {
          return false;
        }
      }
      selector.SetIdentList(std::make_unique<Vector<AtomicString>>(types));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionGroupChildren:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoViewTransitionNew: {
      std::unique_ptr<Vector<AtomicString>> name_and_classes =
          std::make_unique<Vector<AtomicString>>();
      // Is this a view transition class?
      if (stream.Peek().GetType() == kDelimiterToken &&
          stream.Peek().Delimiter() == '.') {
        name_and_classes->push_back(CSSSelector::UniversalSelectorAtom());
      }

      if (name_and_classes->empty()) {
        const CSSParserToken& ident = stream.Peek();
        if (ident.GetType() == kDelimiterToken && ident.Delimiter() == '*') {
          name_and_classes->push_back(CSSSelector::UniversalSelectorAtom());
          stream.Consume();
        } else if (auto* custom_ident = css_parsing_utils::ConsumeCustomIdent(
                       stream, *context_)) {
          name_and_classes->push_back(custom_ident->Value());
        } else {
          return false;
        }
      }

      CHECK_EQ(name_and_classes->size(), 1ull);

      // Parse view transition classes.
      while (!stream.AtEnd() && stream.Peek().GetType() != kWhitespaceToken) {
        if (stream.Peek().GetType() != kDelimiterToken ||
            stream.Consume().Delimiter() != '.') {
          return false;
        }

        CSSCustomIdentValue* custom_ident =
            css_parsing_utils::ConsumeCustomIdent(stream, *context_);
        if (!custom_ident) {
          return false;
        }
        name_and_classes->push_back(custom_ident->Value());
      }

      stream.ConsumeWhitespace();

      if (!stream.AtEnd()) {
        return false;
      }

      selector.SetIdentList(std::move(name_and_classes));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoSlotted: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> inside_compound(&inside_compound_pseudo_, true);

      {
        ResetVectorAfterScope reset_vector(output_);
        base::span<CSSSelector> inner_selector = ConsumeCompoundSelector(
            stream, CSSNestingType::kNone, result_flags);
        stream.ConsumeWhitespace();
        if (inner_selector.empty() || !stream.AtEnd()) {
          return false;
        }
        MarkAsEntireComplexSelector(reset_vector.AddedElements());
        selector.SetSelectorList(
            CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements()));
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoLang: {
      if (!RuntimeEnabledFeatures::CSSLangExtendedRangesEnabled()) {
        const CSSParserToken& ident = stream.Peek();
        if (ident.GetType() != kIdentToken) {
          return false;
        }
        selector.SetArgumentList(std::make_unique<Vector<AtomicString>>(
            Vector<AtomicString>{ident.Value().ToAtomicString()}));
        stream.ConsumeIncludingWhitespace();
        if (!stream.AtEnd()) {
          return false;
        }
        output_.push_back(std::move(selector));
        return true;
      }

      // Per CSS Selectors 4, each language range must be an ident or string.
      // Validation against the BCP47 grammar will happen at match time.
      Vector<AtomicString> langs;

      while (!stream.AtEnd()) {
        const CSSParserToken& lang_token = stream.Peek();

        if (lang_token.GetType() == kIdentToken) {
          langs.push_back(lang_token.Value().ToAtomicString());
          stream.ConsumeIncludingWhitespace();
        } else if (lang_token.GetType() == kStringToken) {
          langs.push_back(lang_token.Value().ToAtomicString());
          stream.ConsumeIncludingWhitespace();
        } else {
          return false;
        }

        if (!stream.AtEnd()) {
          if (stream.Peek().GetType() != kCommaToken) {
            return false;
          }
          stream.ConsumeIncludingWhitespace();
          if (stream.AtEnd()) {
            // Trailing comma.
            return false;
          }
        }
      }

      if (langs.empty()) {
        return false;
      }

      selector.SetArgumentList(
          std::make_unique<Vector<AtomicString>>(std::move(langs)));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastOfType: {
      std::pair<int, int> ab;
      if (!ConsumeANPlusB(stream, ab)) {
        return false;
      }
      stream.ConsumeWhitespace();
      if (stream.AtEnd()) {
        selector.SetNth(ab.first, ab.second, nullptr);
        output_.push_back(std::move(selector));
        return true;
      }

      // See if there's an “of ...” part.
      if (selector.GetPseudoType() != CSSSelector::kPseudoNthChild &&
          selector.GetPseudoType() != CSSSelector::kPseudoNthLastChild) {
        return false;
      }

      CSSSelectorList* sub_selectors = ConsumeNthChildOfSelectors(stream);
      if (sub_selectors == nullptr) {
        return false;
      }
      stream.ConsumeWhitespace();
      if (!stream.AtEnd()) {
        return false;
      }

      selector.SetNth(ab.first, ab.second, sub_selectors);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoOverscrollAreaParent: {
      const CSSParserToken& ident = stream.Peek();
      if (ident.GetType() == kDelimiterToken && ident.Delimiter() == '*') {
        selector.SetArgument(AtomicString("*"));
      } else {
        return false;
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoScrollButton: {
      const CSSParserToken& ident = stream.Peek();
      if (ident.GetType() == kIdentToken) {
        if (!IsScrollButtonDirectionKeyword(ident)) {
          return false;
        }
        selector.SetArgument(ident.Value().ToAtomicString());
      } else if (ident.GetType() == kDelimiterToken &&
                 ident.Delimiter() == '*') {
        selector.SetArgument(AtomicString("*"));
      } else {
        return false;
      }
      stream.ConsumeIncludingWhitespace();
      if (!stream.AtEnd()) {
        return false;
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoHighlight: {
      const CSSParserToken& ident = stream.Peek();
      if (ident.GetType() != kIdentToken) {
        return false;
      }
      selector.SetArgument(ident.Value().ToAtomicString());
      stream.ConsumeIncludingWhitespace();
      if (!stream.AtEnd()) {
        return false;
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoLinkTo:
      if (!RuntimeEnabledFeatures::RouteMatchingEnabled()) {
        return false;
      }
      if (LinkCondition* link_condition =
              LinkConditionParser::Parse(stream, *context_->GetDocument())) {
        selector.SetLinkCondition(link_condition);
        output_.push_back(std::move(selector));
        return true;
      }
      return false;
    default:
      break;
  }

  return false;
}

bool CSSSelectorParser::ConsumeNestingParent(CSSParserTokenStream& stream,
                                             ResultFlags& result_flags) {
  DCHECK_EQ(stream.Peek().GetType(), kDelimiterToken);
  DCHECK_EQ(stream.Peek().Delimiter(), '&');
  stream.Consume();

  output_.push_back(
      CSSSelector(parent_rule_for_nesting_, /*is_implicit=*/false));

  result_flags |= kContainsScopeOrParent;
  // In case that a nesting parent selector is inside a :has() pseudo-class,
  // mark the :has() containing a pseudo selector and a complex selector
  // so that the StyleEngine can invalidate the anchor element of the :has()
  // for a pseudo state change (crbug.com/1517866) or a complex selector
  // state change (crbug.com/350946979) in the parent selector.
  // These ignore whether the nesting parent actually contains a pseudo or
  // complex selector to avoid nesting parent lookup overhead and the
  // complexity caused by reparenting style rules.
  result_flags |= kContainsPseudo;
  result_flags |= kContainsComplexSelector;

  return true;
}

bool CSSSelectorParser::PeekIsCombinator(CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() != kDelimiterToken) {
    return false;
  }

  switch (stream.Peek().Delimiter()) {
    case '+':
    case '~':
    case '>':
      return true;
    default:
      return false;
  }
}

CSSSelector::RelationType CSSSelectorParser::ConsumeCombinator(
    CSSParserTokenStream& stream) {
  CSSSelector::RelationType fallback_result = CSSSelector::kSubSelector;
  while (stream.Peek().GetType() == kWhitespaceToken) {
    stream.Consume();
    fallback_result = CSSSelector::kDescendant;
  }

  if (stream.Peek().GetType() != kDelimiterToken) {
    return fallback_result;
  }

  switch (stream.Peek().Delimiter()) {
    case '+':
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kDirectAdjacent;

    case '~':
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kIndirectAdjacent;

    case '>':
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kChild;

    default:
      break;
  }
  return fallback_result;
}

CSSSelector::MatchType CSSSelectorParser::ConsumeAttributeMatch(
    CSSParserTokenStream& stream) {
  const CSSParserToken& token = stream.Peek();
  switch (token.GetType()) {
    case kIncludeMatchToken:
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kAttributeList;
    case kDashMatchToken:
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kAttributeHyphen;
    case kPrefixMatchToken:
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kAttributeBegin;
    case kSuffixMatchToken:
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kAttributeEnd;
    case kSubstringMatchToken:
      stream.ConsumeIncludingWhitespace();
      return CSSSelector::kAttributeContain;
    case kDelimiterToken:
      if (token.Delimiter() == '=') {
        stream.ConsumeIncludingWhitespace();
        return CSSSelector::kAttributeExact;
      }
      [[fallthrough]];
    default:
      failed_parsing_ = true;
      return CSSSelector::kAttributeExact;
  }
}

CSSSelector::AttributeMatchType CSSSelectorParser::ConsumeAttributeFlags(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken) {
    return CSSSelector::AttributeMatchType::kCaseSensitive;
  }
  const CSSParserToken& flag = stream.ConsumeIncludingWhitespace();
  if (EqualIgnoringASCIICase(flag.Value(), "i")) {
    return CSSSelector::AttributeMatchType::kCaseInsensitive;
  } else if (EqualIgnoringASCIICase(flag.Value(), "s") &&
             RuntimeEnabledFeatures::CSSCaseSensitiveSelectorEnabled()) {
    return CSSSelector::AttributeMatchType::kCaseSensitiveAlways;
  }
  failed_parsing_ = true;
  return CSSSelector::AttributeMatchType::kCaseSensitive;
}

bool CSSSelectorParser::ConsumeANPlusB(CSSParserTokenStream& stream,
                                       std::pair<int, int>& result) {
  if (stream.AtEnd()) {
    return false;
  }

  if (stream.Peek().GetBlockType() != CSSParserToken::kNotBlock) {
    return false;
  }

  const CSSParserToken& token = stream.Consume();
  if (token.GetType() == kNumberToken &&
      token.GetNumericValueType() == kIntegerValueType) {
    result = std::make_pair(0, ClampTo<int>(token.NumericValue()));
    return true;
  }
  if (token.GetType() == kIdentToken) {
    if (EqualIgnoringASCIICase(token.Value(), "odd")) {
      result = std::make_pair(2, 1);
      return true;
    }
    if (EqualIgnoringASCIICase(token.Value(), "even")) {
      result = std::make_pair(2, 0);
      return true;
    }
  }

  // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
  // this will store a string of the form 'n', 'n-', or 'n-123'.
  String n_string;

  if (token.GetType() == kDelimiterToken && token.Delimiter() == '+' &&
      stream.Peek().GetType() == kIdentToken) {
    result.first = 1;
    n_string = stream.Consume().Value().ToString();
  } else if (token.GetType() == kDimensionToken &&
             token.GetNumericValueType() == kIntegerValueType) {
    result.first = ClampTo<int>(token.NumericValue());
    n_string = token.Value().ToString();
  } else if (token.GetType() == kIdentToken) {
    if (token.Value()[0] == '-') {
      result.first = -1;
      n_string = token.Value().ToString().Substring(1);
    } else {
      result.first = 1;
      n_string = token.Value().ToString();
    }
  }

  stream.ConsumeWhitespace();

  if (n_string.empty() || !IsASCIIAlphaCaselessEqual(n_string[0], 'n')) {
    return false;
  }
  if (n_string.length() > 1 && n_string[1] != '-') {
    return false;
  }

  if (n_string.length() > 2) {
    bool valid;
    result.second = n_string.Substring(1).ToIntStrict(&valid);
    return valid;
  }

  NumericSign sign = n_string.length() == 1 ? kNoSign : kMinusSign;
  if (sign == kNoSign && stream.Peek().GetType() == kDelimiterToken) {
    char delimiter_sign = stream.ConsumeIncludingWhitespace().Delimiter();
    if (delimiter_sign == '+') {
      sign = kPlusSign;
    } else if (delimiter_sign == '-') {
      sign = kMinusSign;
    } else {
      return false;
    }
  }

  if (sign == kNoSign && stream.Peek().GetType() != kNumberToken) {
    result.second = 0;
    return true;
  }

  CSSParserToken b = stream.Peek();
  if (b.GetType() != kNumberToken ||
      b.GetNumericValueType() != kIntegerValueType) {
    return false;
  }
  if ((b.GetNumericSign() == kNoSign) == (sign == kNoSign)) {
    return false;
  }
  result.second = ClampTo<int>(b.NumericValue());
  stream.Consume();
  if (sign == kMinusSign) {
    // Negating minimum integer returns itself, instead return max integer.
    if (result.second == std::numeric_limits<int>::min()) [[unlikely]] {
      result.second = std::numeric_limits<int>::max();
    } else {
      result.second = -result.second;
    }
  }
  return true;
}

// Consumes the “of ...” part of :nth_child(An+B of ...).
// Returns nullptr on failure.
CSSSelectorList* CSSSelectorParser::ConsumeNthChildOfSelectors(
    CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() != kIdentToken ||
      stream.Consume().Value() != "of") {
    return nullptr;
  }
  stream.ConsumeWhitespace();

  ResetVectorAfterScope reset_vector(output_);
  ResultFlags result_flags = 0;
  base::span<CSSSelector> selectors =
      ConsumeComplexSelectorList(stream, CSSNestingType::kNone, result_flags);
  if (selectors.empty()) {
    return nullptr;
  }
  return CSSSelectorList::AdoptSelectorVector(selectors);
}

const AtomicString& CSSSelectorParser::DefaultNamespace() const {
  if (!style_sheet_ || ignore_default_namespace_) {
    return g_star_atom;
  }
  return style_sheet_->DefaultNamespace();
}

const AtomicString& CSSSelectorParser::DetermineNamespace(
    const AtomicString& prefix) {
  if (prefix.IsNull()) {
    return DefaultNamespace();
  }
  if (prefix.empty()) {
    return g_empty_atom;  // No namespace. If an element/attribute has a
                          // namespace, we won't match it.
  }
  if (prefix == g_star_atom) {
    return g_star_atom;  // We'll match any namespace.
  }
  if (!style_sheet_) {
    return g_null_atom;  // Cannot resolve prefix to namespace without a
                         // stylesheet, syntax error.
  }
  return style_sheet_->NamespaceURIFromPrefix(prefix);
}

void CSSSelectorParser::PrependTypeSelectorIfNeeded(
    const AtomicString& namespace_prefix,
    bool has_q_name,
    const AtomicString& element_name,
    wtf_size_t start_index_of_compound_selector) {
  const CSSSelector& compound_selector =
      output_[start_index_of_compound_selector];

  if (!has_q_name && DefaultNamespace() == g_star_atom &&
      !NeedsImplicitCombinatorForMatching(compound_selector)) {
    return;
  }

  AtomicString determined_element_name =
      !has_q_name ? CSSSelector::UniversalSelectorAtom() : element_name;
  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull()) {
    failed_parsing_ = true;
    return;
  }
  AtomicString determined_prefix = namespace_prefix;
  if (namespace_uri == DefaultNamespace()) {
    determined_prefix = g_null_atom;
  }
  QualifiedName tag =
      QualifiedName(determined_prefix, determined_element_name, namespace_uri);

  // *:host/*:host-context never matches, so we can't discard the *,
  // otherwise we can't tell the difference between *:host and just :host.
  //
  // Also, selectors where we use a ShadowPseudo combinator between the
  // element and the pseudo-element for matching (custom pseudo-elements,
  // ::cue, ::shadow), we need a universal selector to set the combinator
  // (relation) on in the cases where there are no simple selectors preceding
  // the pseudo-element.
  bool is_host_pseudo = IsHostPseudoSelector(compound_selector);
  if (is_host_pseudo && !has_q_name && namespace_prefix.IsNull()) {
    return;
  }
  if (tag != AnyQName() || is_host_pseudo ||
      NeedsImplicitCombinatorForMatching(compound_selector)) {
    const bool is_implicit =
        determined_prefix == g_null_atom &&
        determined_element_name == CSSSelector::UniversalSelectorAtom() &&
        !is_host_pseudo;

    output_.insert(start_index_of_compound_selector,
                   CSSSelector(tag, is_implicit));
  }
}

// Pseudo-element selectors essentially contain a "built-in" combinator;
// the "foo" and "bar" parts of a selector like `foo::bar` target two
// different elements, just like `foo > bar` would. Due to how CSSSelectors
// are stored in memory (reverse compound order), we sometimes need to create
// an impliit combinator preceding each pseudo-element selector in order
// to start the matching process in the right place. For example:
//
//   .somehost::part(mypart)
//
// This selector should match some element (e.g. <div part=mypart>) inside
// a shadow tree hosted by e.g. <div class=somehost>, and we need to check
// this selector while holding the <div part=mypart> element as the context
// element [1]. However, without a combinator split, this is a single compound
// selector, and selector evaluation would start at the .somehost part,
// which is really targeting a *different element* (the host).
//
// Therefore, we basically rewrite this selector to:
//
//   .somehost >> ::part(mypart)
//
// (Where >> is an imaginary "part" combinator.)
//
// This allows matching to begin with the correct selector (::part(mypart)),
// and we change the context element to the host when processing the '>>'
// combinator.
//
// [1] SelectorCheckingContext::element
void CSSSelectorParser::SplitCompoundAtImplicitCombinator(
    base::span<CSSSelector> selectors) {
  // The simple selectors are stored in an array that stores
  // combinator-separated compound selectors from right-to-left. Yet, within a
  // single compound selector, stores the simple selectors from left-to-right.
  //
  // ".a.b > div#id" is stored as [div, #id, .a, .b], each element in the list
  // stored with an associated relation (combinator or SubSelector).
  //
  // ::cue, ::shadow, and custom pseudo-elements have an implicit ShadowPseudo
  // combinator to their left, which really makes for a new compound selector,
  // yet it's consumed by the selector parser as a single compound selector.
  //
  // Example:
  //
  // input#x::-webkit-clear-button -> [ ::-webkit-clear-button, input, #x ]
  //
  // Likewise, ::slotted() pseudo-element has an implicit ShadowSlot combinator
  // to its left for finding matching slot element in other TreeScope.
  //
  // ::part has a implicit ShadowPart combinator to its left finding the host
  // element in the scope of the style rule.
  //
  // Example:
  //
  // slot[name=foo]::slotted(div) -> [ ::slotted(div), slot, [name=foo] ]
  for (size_t i = 1; i < selectors.size(); ++i) {
    if (NeedsImplicitCombinatorForMatching(selectors[i])) {
      CSSSelector::RelationType relation =
          GetImplicitCombinatorForMatching(selectors[i].GetPseudoType());
      std::rotate(selectors.begin(), selectors.begin() + i, selectors.end());

      base::span<CSSSelector> remaining = selectors.first(selectors.size() - i);
      // We might need to split the compound multiple times, since a number of
      // the relevant pseudo-elements can be combined, and they all need an
      // implicit combinator for matching.
      SplitCompoundAtImplicitCombinator(remaining);
      remaining.back().SetRelation(relation);
      break;
    }
  }
}

namespace {

struct PseudoElementFeatureMapEntry {
  template <unsigned key_length>
  PseudoElementFeatureMapEntry(const char (&key)[key_length],
                               WebFeature feature)
      : key(key),
        key_length(base::checked_cast<uint16_t>(key_length - 1)),
        feature(base::checked_cast<uint16_t>(feature)) {}
  const char* const key;
  const uint16_t key_length;
  const uint16_t feature;
};

WebFeature FeatureForWebKitCustomPseudoElement(const AtomicString& name) {
  static const PseudoElementFeatureMapEntry feature_table[] = {
      {"cue", WebFeature::kCSSSelectorCue},
      {"-internal-media-controls-overlay-cast-button",
       WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton},
      {"-webkit-calendar-picker-indicator",
       WebFeature::kCSSSelectorWebkitCalendarPickerIndicator},
      {"-webkit-clear-button", WebFeature::kCSSSelectorWebkitClearButton},
      {"-webkit-color-swatch", WebFeature::kCSSSelectorWebkitColorSwatch},
      {"-webkit-color-swatch-wrapper",
       WebFeature::kCSSSelectorWebkitColorSwatchWrapper},
      {"-webkit-date-and-time-value",
       WebFeature::kCSSSelectorWebkitDateAndTimeValue},
      {"-webkit-datetime-edit", WebFeature::kCSSSelectorWebkitDatetimeEdit},
      {"-webkit-datetime-edit-ampm-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditAmpmField},
      {"-webkit-datetime-edit-day-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditDayField},
      {"-webkit-datetime-edit-fields-wrapper",
       WebFeature::kCSSSelectorWebkitDatetimeEditFieldsWrapper},
      {"-webkit-datetime-edit-hour-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditHourField},
      {"-webkit-datetime-edit-millisecond-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMillisecondField},
      {"-webkit-datetime-edit-minute-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMinuteField},
      {"-webkit-datetime-edit-month-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMonthField},
      {"-webkit-datetime-edit-second-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditSecondField},
      {"-webkit-datetime-edit-text",
       WebFeature::kCSSSelectorWebkitDatetimeEditText},
      {"-webkit-datetime-edit-week-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditWeekField},
      {"-webkit-datetime-edit-year-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditYearField},
      {"-webkit-file-upload-button",
       WebFeature::kCSSSelectorWebkitFileUploadButton},
      {"-webkit-inner-spin-button",
       WebFeature::kCSSSelectorWebkitInnerSpinButton},
      {"-webkit-input-placeholder",
       WebFeature::kCSSSelectorWebkitInputPlaceholder},
      {"-webkit-media-controls", WebFeature::kCSSSelectorWebkitMediaControls},
      {"-webkit-media-controls-current-time-display",
       WebFeature::kCSSSelectorWebkitMediaControlsCurrentTimeDisplay},
      {"-webkit-media-controls-enclosure",
       WebFeature::kCSSSelectorWebkitMediaControlsEnclosure},
      {"-webkit-media-controls-fullscreen-button",
       WebFeature::kCSSSelectorWebkitMediaControlsFullscreenButton},
      {"-webkit-media-controls-mute-button",
       WebFeature::kCSSSelectorWebkitMediaControlsMuteButton},
      {"-webkit-media-controls-overlay-enclosure",
       WebFeature::kCSSSelectorWebkitMediaControlsOverlayEnclosure},
      {"-webkit-media-controls-overlay-play-button",
       WebFeature::kCSSSelectorWebkitMediaControlsOverlayPlayButton},
      {"-webkit-media-controls-panel",
       WebFeature::kCSSSelectorWebkitMediaControlsPanel},
      {"-webkit-media-controls-play-button",
       WebFeature::kCSSSelectorWebkitMediaControlsPlayButton},
      {"-webkit-media-controls-timeline",
       WebFeature::kCSSSelectorWebkitMediaControlsTimeline},
      // Note: This feature is no longer implemented in Blink.
      {"-webkit-media-controls-timeline-container",
       WebFeature::kCSSSelectorWebkitMediaControlsTimelineContainer},
      {"-webkit-media-controls-time-remaining-display",
       WebFeature::kCSSSelectorWebkitMediaControlsTimeRemainingDisplay},
      {"-webkit-media-controls-toggle-closed-captions-button",
       WebFeature::kCSSSelectorWebkitMediaControlsToggleClosedCaptionsButton},
      {"-webkit-media-controls-volume-slider",
       WebFeature::kCSSSelectorWebkitMediaControlsVolumeSlider},
      {"-webkit-media-slider-container",
       WebFeature::kCSSSelectorWebkitMediaSliderContainer},
      {"-webkit-media-slider-thumb",
       WebFeature::kCSSSelectorWebkitMediaSliderThumb},
      {"-webkit-media-text-track-container",
       WebFeature::kCSSSelectorWebkitMediaTextTrackContainer},
      {"-webkit-media-text-track-display",
       WebFeature::kCSSSelectorWebkitMediaTextTrackDisplay},
      {"-webkit-media-text-track-region",
       WebFeature::kCSSSelectorWebkitMediaTextTrackRegion},
      {"-webkit-media-text-track-region-container",
       WebFeature::kCSSSelectorWebkitMediaTextTrackRegionContainer},
      {"-webkit-meter-bar", WebFeature::kCSSSelectorWebkitMeterBar},
      {"-webkit-meter-even-less-good-value",
       WebFeature::kCSSSelectorWebkitMeterEvenLessGoodValue},
      {"-webkit-meter-inner-element",
       WebFeature::kCSSSelectorWebkitMeterInnerElement},
      {"-webkit-meter-optimum-value",
       WebFeature::kCSSSelectorWebkitMeterOptimumValue},
      {"-webkit-meter-suboptimum-value",
       WebFeature::kCSSSelectorWebkitMeterSuboptimumValue},
      {"-webkit-progress-bar", WebFeature::kCSSSelectorWebkitProgressBar},
      {"-webkit-progress-inner-element",
       WebFeature::kCSSSelectorWebkitProgressInnerElement},
      {"-webkit-progress-value", WebFeature::kCSSSelectorWebkitProgressValue},
      {"-webkit-search-cancel-button",
       WebFeature::kCSSSelectorWebkitSearchCancelButton},
      {"-webkit-slider-container",
       WebFeature::kCSSSelectorWebkitSliderContainer},
      {"-webkit-slider-runnable-track",
       WebFeature::kCSSSelectorWebkitSliderRunnableTrack},
      {"-webkit-slider-thumb", WebFeature::kCSSSelectorWebkitSliderThumb},
      {"-webkit-textfield-decoration-container",
       WebFeature::kCSSSelectorWebkitTextfieldDecorationContainer},
  };
  // TODO(fs): Could use binary search once there's a less finicky way to
  // compare (order) String and StringView/non-String.
  for (const auto& entry : feature_table) {
    // SAFETY: The PseudoElementFeatureMapEntry constructor guarantees `key` and
    // `key_length` are safe.
    if (name == StringView(base::as_bytes(
                    UNSAFE_BUFFERS(base::span(entry.key, entry.key_length))))) {
      return static_cast<WebFeature>(entry.feature);
    }
  }
  return WebFeature::kCSSSelectorWebkitUnknownPseudo;
}

}  // namespace

static void RecordUsageAndDeprecationsOneSelector(
    const CSSSelector* selector,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    bool* has_visited_pseudo) {
  // Both the classic WebFeature and the newer WebDXFeature use counters can be
  // recorded. Some WebFeature counters are mapped to WebDXFeature counters in
  // webdx_feature_maps.cc. For new features, it's OK to only add a WebDXFeature
  // counter instead of adding a classic counter and mapping it.
  std::optional<WebFeature> feature;
  std::optional<WebDXFeature> webdx_feature;
  switch (selector->GetPseudoType()) {
    case CSSSelector::kPseudoAny:
      feature = WebFeature::kCSSSelectorPseudoAny;
      break;
    case CSSSelector::kPseudoIs:
      feature = WebFeature::kCSSSelectorPseudoIs;
      break;
    case CSSSelector::kPseudoFocusVisible:
      feature = WebFeature::kCSSSelectorPseudoFocusVisible;
      break;
    case CSSSelector::kPseudoFocus:
      feature = WebFeature::kCSSSelectorPseudoFocus;
      break;
    case CSSSelector::kPseudoAnyLink:
      feature = WebFeature::kCSSSelectorPseudoAnyLink;
      break;
    case CSSSelector::kPseudoWebkitAnyLink:
      feature = WebFeature::kCSSSelectorPseudoWebkitAnyLink;
      break;
    case CSSSelector::kPseudoWhere:
      feature = WebFeature::kCSSSelectorPseudoWhere;
      break;
    case CSSSelector::kPseudoDefined:
      feature = WebFeature::kCSSSelectorPseudoDefined;
      break;
    case CSSSelector::kPseudoSlotted:
      feature = WebFeature::kCSSSelectorPseudoSlotted;
      break;
    case CSSSelector::kPseudoHost:
      feature = WebFeature::kCSSSelectorPseudoHost;
      break;
    case CSSSelector::kPseudoHostContext:
      feature = WebFeature::kCSSSelectorPseudoHostContext;
      break;
    case CSSSelector::kPseudoFullScreenAncestor:
      feature = WebFeature::kCSSSelectorPseudoFullScreenAncestor;
      break;
    case CSSSelector::kPseudoFullScreen:
      feature = WebFeature::kCSSSelectorPseudoFullScreen;
      break;
    case CSSSelector::kPseudoListBox:
      feature = WebFeature::kCSSSelectorInternalPseudoListBox;
      break;
    case CSSSelector::kPseudoWebKitCustomElement:
      feature = FeatureForWebKitCustomPseudoElement(selector->Value());
      break;
    case CSSSelector::kPseudoSpatialNavigationFocus:
      feature = WebFeature::kCSSSelectorInternalPseudoSpatialNavigationFocus;
      break;
    case CSSSelector::kPseudoReadOnly:
      feature = WebFeature::kCSSSelectorPseudoReadOnly;
      break;
    case CSSSelector::kPseudoReadWrite:
      feature = WebFeature::kCSSSelectorPseudoReadWrite;
      break;
    case CSSSelector::kPseudoDir:
      feature = WebFeature::kCSSSelectorPseudoDir;
      break;
    case CSSSelector::kPseudoHas:
      feature = WebFeature::kCSSSelectorPseudoHas;
      break;
    case CSSSelector::kPseudoHasSlotted:
      feature = WebFeature::kCSSSelectorPseudoHasSlotted;
      break;
    case CSSSelector::kPseudoState:
      feature = WebFeature::kCSSSelectorPseudoState;
      break;
    case CSSSelector::kPseudoUserValid:
      feature = WebFeature::kCSSSelectorUserValid;
      break;
    case CSSSelector::kPseudoUserInvalid:
      feature = WebFeature::kCSSSelectorUserInvalid;
      break;
    case CSSSelector::kPseudoNthChild:
      if (selector->SelectorList()) {
        feature = WebFeature::kCSSSelectorNthChildOfSelector;
      }
      break;
    case CSSSelector::kPseudoModal:
      feature = WebFeature::kCSSSelectorPseudoModal;
      break;
    case CSSSelector::kPseudoFileSelectorButton:
      feature = WebFeature::kCSSSelectorPseudoFileSelectorButton;
      break;
    case CSSSelector::kPseudoVisited:
      if (has_visited_pseudo) {
        *has_visited_pseudo = true;
      }
      break;
    case CSSSelector::kPseudoActiveViewTransition:
      feature = WebFeature::kActiveViewTransitionPseudo;
      break;
    case CSSSelector::kPseudoOpen:
      feature = WebFeature::kCSSPseudoOpen;
      break;
    case CSSSelector::kPseudoNot:
      feature = WebFeature::kCSSSelectorPseudoNot;
      break;
    case CSSSelector::kPseudoAutofill:
      webdx_feature = WebDXFeature::kAutofill;
      break;
    case CSSSelector::kPseudoDetailsContent:
      webdx_feature = WebDXFeature::kDetailsContent;
      break;
    case CSSSelector::kPseudoPastCue:
    case CSSSelector::kPseudoFutureCue:
      webdx_feature = WebDXFeature::kTimeRelativeSelectors;
      break;
    case CSSSelector::kPseudoParent:
      if (nesting_type == CSSNestingType::kScope) {
        feature = WebFeature::kCSSPseudoParentInScope;
      }
      break;
    default:
      break;
  }
  if (feature.has_value()) {
    if (Deprecation::IsDeprecated(*feature)) {
      context->CountDeprecation(*feature);
    } else {
      context->Count(*feature);
    }
  }
  if (webdx_feature.has_value()) {
    context->Count(*webdx_feature);
  }
  if (selector->Relation() == CSSSelector::kIndirectAdjacent) {
    context->Count(WebFeature::kCSSSelectorIndirectAdjacent);
  }
  if (selector->SelectorList()) {
    for (const CSSSelector* current = selector->SelectorList()->First();
         current; current = current->NextSimpleSelector()) {
      RecordUsageAndDeprecationsOneSelector(current, context, nesting_type,
                                            has_visited_pseudo);
    }
  }
}

void CSSSelectorParser::RecordUsageAndDeprecations(
    const base::span<CSSSelector> selector_vector,
    CSSNestingType nesting_type,
    bool* has_visited_pseudo) {
  if (!context_->IsUseCounterRecordingEnabled()) {
    return;
  }
  if (context_->Mode() == kUASheetMode) {
    return;
  }

  for (const CSSSelector& current : selector_vector) {
    RecordUsageAndDeprecationsOneSelector(&current, context_, nesting_type,
                                          has_visited_pseudo);
  }
}

bool CSSSelectorParser::ContainsUnknownWebkitPseudoElements(
    base::span<CSSSelector> selectors) {
  for (const CSSSelector& current : selectors) {
    if (current.GetPseudoType() != CSSSelector::kPseudoWebKitCustomElement) {
      continue;
    }
    WebFeature feature = FeatureForWebKitCustomPseudoElement(current.Value());
    if (feature == WebFeature::kCSSSelectorWebkitUnknownPseudo) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
