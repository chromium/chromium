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

CSSParserSelector::CSSParserSelector()
    : selector_(std::make_unique<CSSSelector>()) {}

CSSParserSelector::CSSParserSelector(const QualifiedName& tag_q_name,
                                     bool is_implicit)
    : selector_(std::make_unique<CSSSelector>(tag_q_name, is_implicit)) {}

CSSParserSelector::~CSSParserSelector() {
  while (tag_history_) {
    std::unique_ptr<CSSParserSelector> next =
        std::move(tag_history_->tag_history_);
    tag_history_ = std::move(next);
  }
}

void CSSParserSelector::AdoptSelectorVector(
    Vector<std::unique_ptr<CSSParserSelector>>& selector_vector) {
  CSSSelectorList* selector_list = new CSSSelectorList(
      CSSSelectorList::AdoptSelectorVector(selector_vector));
  selector_->SetSelectorList(base::WrapUnique(selector_list));
}

void CSSParserSelector::SetSelectorList(
    std::unique_ptr<CSSSelectorList> selector_list) {
  selector_->SetSelectorList(std::move(selector_list));
}

bool CSSParserSelector::IsSimple() const {
  if (selector_->SelectorList() ||
      selector_->Match() == CSSSelector::kPseudoElement)
    return false;

  if (!tag_history_)
    return true;

  if (selector_->Match() == CSSSelector::kTag) {
    // We can't check against anyQName() here because namespace may not be
    // g_null_atom.
    // Example:
    //     @namespace "http://www.w3.org/2000/svg";
    //     svg:not(:root) { ...
    if (selector_->TagQName().LocalName() ==
        CSSSelector::UniversalSelectorAtom())
      return tag_history_->IsSimple();
  }

  return false;
}

void CSSParserSelector::AppendTagHistory(
    CSSSelector::RelationType relation,
    std::unique_ptr<CSSParserSelector> selector) {
  CSSParserSelector* end = this;
  while (end->TagHistory())
    end = end->TagHistory();
  end->SetRelation(relation);
  end->SetTagHistory(std::move(selector));
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::ReleaseTagHistory() {
  SetRelation(CSSSelector::kSubSelector);
  return std::move(tag_history_);
}

void CSSParserSelector::PrependTagSelector(const QualifiedName& tag_q_name,
                                           bool is_implicit) {
  std::unique_ptr<CSSParserSelector> second =
      std::make_unique<CSSParserSelector>();
  second->selector_ = std::move(selector_);
  second->tag_history_ = std::move(tag_history_);
  tag_history_ = std::move(second);
  selector_ = std::make_unique<CSSSelector>(tag_q_name, is_implicit);
}

bool CSSParserSelector::IsHostPseudoSelector() const {
  return GetPseudoType() == CSSSelector::kPseudoHost ||
         GetPseudoType() == CSSSelector::kPseudoHostContext;
}

RelationType CSSParserSelector::GetImplicitShadowCombinatorForMatching() const {
  switch (GetPseudoType()) {
    case PseudoType::kPseudoSlotted:
      return RelationType::kShadowSlot;
    case PseudoType::kPseudoWebKitCustomElement:
    case PseudoType::kPseudoBlinkInternalElement:
    case PseudoType::kPseudoCue:
    case PseudoType::kPseudoPlaceholder:
    case PseudoType::kPseudoShadow:
      return RelationType::kShadowPseudo;
    case PseudoType::kPseudoPart:
      return RelationType::kShadowPart;
    default:
      return RelationType::kSubSelector;
  }
}

bool CSSParserSelector::NeedsImplicitShadowCombinatorForMatching() const {
  return GetImplicitShadowCombinatorForMatching() != RelationType::kSubSelector;
}

}  // namespace blink
