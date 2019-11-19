// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"

#include <stdint.h>

#include "base/i18n/case_conversion.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace content {

// Given a node, populate a vector with all of the strings from that node's
// attributes that might be relevant for a text search.
void GetNodeStrings(BrowserAccessibility* node,
                    std::vector<base::string16>* strings) {
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kName))
    strings->push_back(
        node->GetString16Attribute(ax::mojom::StringAttribute::kName));
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kDescription))
    strings->push_back(
        node->GetString16Attribute(ax::mojom::StringAttribute::kDescription));
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kValue))
    strings->push_back(
        node->GetString16Attribute(ax::mojom::StringAttribute::kValue));
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder))
    strings->push_back(
        node->GetString16Attribute(ax::mojom::StringAttribute::kPlaceholder));
}

OneShotAccessibilityTreeSearch::OneShotAccessibilityTreeSearch(
    BrowserAccessibility* scope)
    : tree_(scope->manager()),
      scope_node_(scope),
      start_node_(scope),
      direction_(OneShotAccessibilityTreeSearch::FORWARDS),
      result_limit_(UNLIMITED_RESULTS),
      immediate_descendants_only_(false),
      can_wrap_to_last_element_(false),
      onscreen_only_(false),
      did_search_(false) {}

OneShotAccessibilityTreeSearch::~OneShotAccessibilityTreeSearch() {}

void OneShotAccessibilityTreeSearch::SetStartNode(
    BrowserAccessibility* start_node) {
  DCHECK(!did_search_);
  CHECK(start_node);

  if (!scope_node_->PlatformGetParent() ||
      start_node->IsDescendantOf(scope_node_->PlatformGetParent())) {
    start_node_ = start_node;
  }
}

void OneShotAccessibilityTreeSearch::SetDirection(Direction direction) {
  DCHECK(!did_search_);
  direction_ = direction;
}

void OneShotAccessibilityTreeSearch::SetResultLimit(int result_limit) {
  DCHECK(!did_search_);
  result_limit_ = result_limit;
}

void OneShotAccessibilityTreeSearch::SetImmediateDescendantsOnly(
    bool immediate_descendants_only) {
  DCHECK(!did_search_);
  immediate_descendants_only_ = immediate_descendants_only;
}

void OneShotAccessibilityTreeSearch::SetCanWrapToLastElement(
    bool can_wrap_to_last_element) {
  DCHECK(!did_search_);
  can_wrap_to_last_element_ = can_wrap_to_last_element;
}

void OneShotAccessibilityTreeSearch::SetOnscreenOnly(bool onscreen_only) {
  DCHECK(!did_search_);
  onscreen_only_ = onscreen_only;
}

void OneShotAccessibilityTreeSearch::SetSearchText(const std::string& text) {
  DCHECK(!did_search_);
  search_text_ = text;
}

void OneShotAccessibilityTreeSearch::AddPredicate(
    AccessibilityMatchPredicate predicate) {
  DCHECK(!did_search_);
  predicates_.push_back(predicate);
}

size_t OneShotAccessibilityTreeSearch::CountMatches() {
  if (!did_search_)
    Search();

  return matches_.size();
}

BrowserAccessibility* OneShotAccessibilityTreeSearch::GetMatchAtIndex(
    size_t index) {
  if (!did_search_)
    Search();

  CHECK(index < matches_.size());
  return matches_[index];
}

void OneShotAccessibilityTreeSearch::Search() {
  if (immediate_descendants_only_) {
    SearchByIteratingOverChildren();
  } else {
    SearchByWalkingTree();
  }
  did_search_ = true;
}

void OneShotAccessibilityTreeSearch::SearchByIteratingOverChildren() {
  // Iterate over the children of scope_node_.
  // If start_node_ is specified, iterate over the first child past that
  // node.

  uint32_t count = scope_node_->PlatformChildCount();
  if (count == 0)
    return;

  // We only care about immediate children of scope_node_, so walk up
  // start_node_ until we get to an immediate child. If it isn't a child,
  // we ignore start_node_.
  while (start_node_ && start_node_->PlatformGetParent() != scope_node_)
    start_node_ = start_node_->PlatformGetParent();

  uint32_t index = (direction_ == FORWARDS ? 0 : count - 1);
  if (start_node_) {
    index = start_node_->GetIndexInParent();
    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }

  while (index < count && (result_limit_ == UNLIMITED_RESULTS ||
                           static_cast<int>(matches_.size()) < result_limit_)) {
    BrowserAccessibility* node = scope_node_->PlatformGetChild(index);
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }
}

void OneShotAccessibilityTreeSearch::SearchByWalkingTree() {
  BrowserAccessibility* node = nullptr;
  node = start_node_;
  if (node != scope_node_ || result_limit_ == 1) {
    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(start_node_);
    else
      node = tree_->PreviousInTreeOrder(start_node_, can_wrap_to_last_element_);
  }

  BrowserAccessibility* stop_node = scope_node_->PlatformGetParent();
  while (node && node != stop_node &&
         (result_limit_ == UNLIMITED_RESULTS ||
          static_cast<int>(matches_.size()) < result_limit_)) {
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS) {
      node = tree_->NextInTreeOrder(node);
    } else {
      // This needs to be handled carefully. If not, there is a chance of
      // getting into infinite loop.
      if (can_wrap_to_last_element_ && !stop_node &&
          node->manager()->GetRoot() == node) {
        stop_node = node;
      }
      node = tree_->PreviousInTreeOrder(node, can_wrap_to_last_element_);
    }
  }
}

bool OneShotAccessibilityTreeSearch::Matches(BrowserAccessibility* node) {
  for (size_t i = 0; i < predicates_.size(); ++i) {
    if (!predicates_[i](start_node_, node))
      return false;
  }

  if (node->HasState(ax::mojom::State::kInvisible))
    return false;  // Programmatically hidden, e.g. aria-hidden or via CSS.

  if (onscreen_only_ && node->IsOffscreen())
    return false;  // Partly scrolled offscreen.

  if (!search_text_.empty()) {
    base::string16 search_text_lower =
        base::i18n::ToLower(base::UTF8ToUTF16(search_text_));
    std::vector<base::string16> node_strings;
    GetNodeStrings(node, &node_strings);
    bool found_text_match = false;
    for (size_t i = 0; i < node_strings.size(); ++i) {
      base::string16 node_string_lower = base::i18n::ToLower(node_strings[i]);
      if (node_string_lower.find(search_text_lower) != base::string16::npos) {
        found_text_match = true;
        break;
      }
    }
    if (!found_text_match)
      return false;
  }

  return true;
}

//
// Predicates
//

bool AccessibilityArticlePredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kArticle;
}

bool AccessibilityButtonPredicate(BrowserAccessibility* start,
                                  BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kToggleButton:
      return true;
    default:
      return false;
  }
}

bool AccessibilityBlockquotePredicate(BrowserAccessibility* start,
                                      BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kBlockquote;
}

bool AccessibilityCheckboxPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kCheckBox ||
          node->GetRole() == ax::mojom::Role::kMenuItemCheckBox);
}

bool AccessibilityComboboxPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kComboBoxGrouping ||
          node->GetRole() == ax::mojom::Role::kComboBoxMenuButton ||
          node->GetRole() == ax::mojom::Role::kTextFieldWithComboBox ||
          node->GetRole() == ax::mojom::Role::kPopUpButton);
}

bool AccessibilityControlPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  if (ui::IsControl(node->GetRole()))
    return true;
  if (node->HasState(ax::mojom::State::kFocusable) &&
      node->GetRole() != ax::mojom::Role::kIframe &&
      node->GetRole() != ax::mojom::Role::kIframePresentational &&
      !ui::IsLink(node->GetRole()) &&
      node->GetRole() != ax::mojom::Role::kWebArea &&
      node->GetRole() != ax::mojom::Role::kRootWebArea) {
    return true;
  }
  return false;
}

bool AccessibilityFocusablePredicate(BrowserAccessibility* start,
                                     BrowserAccessibility* node) {
  bool focusable = node->HasState(ax::mojom::State::kFocusable);
  if (node->GetRole() == ax::mojom::Role::kIframe ||
      node->GetRole() == ax::mojom::Role::kIframePresentational ||
      node->GetRole() == ax::mojom::Role::kWebArea ||
      node->GetRole() == ax::mojom::Role::kRootWebArea) {
    focusable = false;
  }
  return focusable;
}

bool AccessibilityGraphicPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return ui::IsImageOrVideo(node->GetRole());
}

bool AccessibilityHeadingPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return ui::IsHeading(node->GetRole());
}

bool AccessibilityH1Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (ui::IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              1);
}

bool AccessibilityH2Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (ui::IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              2);
}

bool AccessibilityH3Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (ui::IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              3);
}

bool AccessibilityH4Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (ui::IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              4);
}

bool AccessibilityH5Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (ui::IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              5);
}

bool AccessibilityH6Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (ui::IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              6);
}

bool AccessibilityHeadingSameLevelPredicate(BrowserAccessibility* start,
                                            BrowserAccessibility* node) {
  return (
      node->GetRole() == ax::mojom::Role::kHeading &&
      start->GetRole() == ax::mojom::Role::kHeading &&
      (node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
       start->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel)));
}

bool AccessibilityFramePredicate(BrowserAccessibility* start,
                                 BrowserAccessibility* node) {
  if (node->IsWebAreaForPresentationalIframe())
    return false;
  if (!node->PlatformGetParent())
    return false;
  return (node->GetRole() == ax::mojom::Role::kWebArea ||
          node->GetRole() == ax::mojom::Role::kRootWebArea);
}

bool AccessibilityLandmarkPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ax::mojom::Role::kApplication:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSearch:
    case ax::mojom::Role::kSection:
      return true;
    default:
      return false;
  }
}

bool AccessibilityLinkPredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return ui::IsLink(node->GetRole());
}

bool AccessibilityListPredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return ui::IsList(node->GetRole());
}

bool AccessibilityListItemPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  return ui::IsListItem(node->GetRole());
}

bool AccessibilityLiveRegionPredicate(BrowserAccessibility* start,
                                      BrowserAccessibility* node) {
  return node->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
}

bool AccessibilityMainPredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kMain);
}

bool AccessibilityMediaPredicate(BrowserAccessibility* start,
                                 BrowserAccessibility* node) {
  const std::string& tag =
      node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  return tag == "audio" || tag == "video";
}

bool AccessibilityPopupButtonPredicate(BrowserAccessibility* start,
                                       BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kPopUpButton);
}

bool AccessibilityRadioButtonPredicate(BrowserAccessibility* start,
                                       BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kRadioButton ||
          node->GetRole() == ax::mojom::Role::kMenuItemRadio);
}

bool AccessibilityRadioGroupPredicate(BrowserAccessibility* start,
                                      BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kRadioGroup;
}

bool AccessibilityTablePredicate(BrowserAccessibility* start,
                                 BrowserAccessibility* node) {
  return ui::IsTableLike(node->GetRole());
}

bool AccessibilityTextfieldPredicate(BrowserAccessibility* start,
                                     BrowserAccessibility* node) {
  return (node->IsPlainTextField() || node->IsRichTextField());
}

bool AccessibilityTextStyleBoldPredicate(BrowserAccessibility* start,
                                         BrowserAccessibility* node) {
  return node->GetData().HasTextStyle(ax::mojom::TextStyle::kBold);
}

bool AccessibilityTextStyleItalicPredicate(BrowserAccessibility* start,
                                           BrowserAccessibility* node) {
  return node->GetData().HasTextStyle(ax::mojom::TextStyle::kItalic);
}

bool AccessibilityTextStyleUnderlinePredicate(BrowserAccessibility* start,
                                              BrowserAccessibility* node) {
  return node->GetData().HasTextStyle(ax::mojom::TextStyle::kUnderline);
}

bool AccessibilityTreePredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return (node->IsPlainTextField() || node->IsRichTextField());
}

bool AccessibilityUnvisitedLinkPredicate(BrowserAccessibility* start,
                                         BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kLink &&
         !node->HasState(ax::mojom::State::kVisited);
}

bool AccessibilityVisitedLinkPredicate(BrowserAccessibility* start,
                                       BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kLink &&
         node->HasState(ax::mojom::State::kVisited);
}

}  // namespace content
