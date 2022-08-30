/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/parser/css_parser_selector.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"

namespace blink {

using RelationType = CSSSelector::RelationType;
using PseudoType = CSSSelector::PseudoType;

template <bool UseArena>
CSSParserSelector<UseArena>::CSSParserSelector(MaybeArena arena) {
  if constexpr (UseArena) {
    selector_.reset(arena.template New<CSSSelector>());
  } else {
    selector_ = std::make_unique<CSSSelector>();
  }
}

template <bool UseArena>
CSSParserSelector<UseArena>::CSSParserSelector(MaybeArena arena,
                                               const QualifiedName& tag_q_name,
                                               bool is_implicit) {
  if constexpr (UseArena) {
    selector_.reset(arena.template New<CSSSelector>(tag_q_name, is_implicit));
  } else {
    selector_ = std::make_unique<CSSSelector>(tag_q_name, is_implicit);
  }
}

template <bool UseArena>
CSSParserSelector<UseArena>::~CSSParserSelector() {
  while (tag_history_) {
    MaybeArenaUniquePtr<CSSParserSelector, UseArena> next =
        std::move(tag_history_->tag_history_);
    tag_history_ = std::move(next);
  }
}

template <bool UseArena>
void CSSParserSelector<UseArena>::AdoptSelectorVector(
    CSSSelectorVector<UseArena>& selector_vector) {
  CSSSelectorList* selector_list = new CSSSelectorList(
      CSSSelectorList::AdoptSelectorVector<UseArena>(selector_vector));
  selector_->SetSelectorList(base::WrapUnique(selector_list));
}

template <bool UseArena>
void CSSParserSelector<UseArena>::SetSelectorList(
    std::unique_ptr<CSSSelectorList> selector_list) {
  selector_->SetSelectorList(std::move(selector_list));
}

template <bool UseArena>
void CSSParserSelector<UseArena>::SetContainsPseudoInsideHasPseudoClass() {
  selector_->SetContainsPseudoInsideHasPseudoClass();
}

template <bool UseArena>
void CSSParserSelector<
    UseArena>::SetContainsComplexLogicalCombinationsInsideHasPseudoClass() {
  selector_->SetContainsComplexLogicalCombinationsInsideHasPseudoClass();
}

template <bool UseArena>
void CSSParserSelector<UseArena>::AppendTagHistory(
    CSSSelector::RelationType relation,
    MaybeArenaUniquePtr<CSSParserSelector, UseArena> selector) {
  CSSParserSelector<UseArena>* end = this;
  while (end->TagHistory())
    end = end->TagHistory();
  end->SetRelation(relation);
  end->SetTagHistory(std::move(selector));
}

template <bool UseArena>
MaybeArenaUniquePtr<CSSParserSelector<UseArena>, UseArena>
CSSParserSelector<UseArena>::ReleaseTagHistory() {
  SetRelation(CSSSelector::kSubSelector);
  return std::move(tag_history_);
}

template <bool UseArena>
void CSSParserSelector<UseArena>::PrependTagSelector(
    Arena& arena,
    const QualifiedName& tag_q_name,
    bool is_implicit) {
  MaybeArenaUniquePtr<CSSParserSelector, UseArena> second;
  if constexpr (UseArena) {
    second.reset(arena.New<CSSParserSelector<true>>(arena));
  } else {
    constexpr int kDummyInt = 0;
    second = std::make_unique<CSSParserSelector<false>>(kDummyInt);
  }
  second->selector_ = std::move(selector_);
  second->tag_history_ = std::move(tag_history_);
  tag_history_ = std::move(second);
  if constexpr (UseArena) {
    selector_.reset(arena.New<CSSSelector>(tag_q_name, is_implicit));
  } else {
    selector_ = std::make_unique<CSSSelector>(tag_q_name, is_implicit);
  }
}

template <bool UseArena>
bool CSSParserSelector<UseArena>::IsHostPseudoSelector() const {
  return GetPseudoType() == CSSSelector::kPseudoHost ||
         GetPseudoType() == CSSSelector::kPseudoHostContext;
}

template <bool UseArena>
RelationType
CSSParserSelector<UseArena>::GetImplicitShadowCombinatorForMatching() const {
  switch (GetPseudoType()) {
    case PseudoType::kPseudoSlotted:
      return RelationType::kShadowSlot;
    case PseudoType::kPseudoWebKitCustomElement:
    case PseudoType::kPseudoBlinkInternalElement:
    case PseudoType::kPseudoCue:
    case PseudoType::kPseudoPlaceholder:
    case PseudoType::kPseudoFileSelectorButton:
      return RelationType::kUAShadow;
    case PseudoType::kPseudoPart:
      return RelationType::kShadowPart;
    default:
      return RelationType::kSubSelector;
  }
}

template <bool UseArena>
bool CSSParserSelector<UseArena>::NeedsImplicitShadowCombinatorForMatching()
    const {
  return GetImplicitShadowCombinatorForMatching() != RelationType::kSubSelector;
}

// Explicit instantiations.
template class CSSParserSelector<false>;
template class CSSParserSelector<true>;

}  // namespace blink
