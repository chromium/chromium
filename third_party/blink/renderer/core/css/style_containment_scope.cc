// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_containment_scope.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"

namespace blink {

void StyleContainmentScope::Trace(Visitor* visitor) const {
  visitor->Trace(quotes_);
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(element_);
}

// If the scope is about to be removed, detach self from the parent,
// reattach the quotes and the children scopes to the parent scope.
void StyleContainmentScope::ReattachToParent() {
  if (parent_) {
    auto quotes = std::move(quotes_);
    for (LayoutQuote* quote : quotes) {
      quote->SetScope(nullptr);
      parent_->AttachQuote(*quote);
    }
    auto children = std::move(children_);
    for (StyleContainmentScope* child : children) {
      child->SetParent(nullptr);
      parent_->AppendChild(child);
    }
    parent_->RemoveChild(this);
  }
}

bool StyleContainmentScope::IsAncestorOf(const Element* element,
                                         const Element* stay_within) {
  for (const Element* it = element; it && it != stay_within;
       it = LayoutTreeBuilderTraversal::ParentElement(*it)) {
    if (it == GetElement()) {
      return true;
    }
  }
  return false;
}

void StyleContainmentScope::AppendChild(StyleContainmentScope* child) {
  DCHECK(!child->Parent());
  children_.emplace_back(child);
  child->SetParent(this);
}

void StyleContainmentScope::RemoveChild(StyleContainmentScope* child) {
  DCHECK_EQ(this, child->Parent());
  wtf_size_t pos = children_.Find(child);
  DCHECK_NE(pos, kNotFound);
  children_.EraseAt(pos);
  child->SetParent(nullptr);
}

// Get the quote which would be the last in preorder traversal before we hit
// Element*.
const LayoutQuote* StyleContainmentScope::FindQuotePrecedingElement(
    const Element& element) const {
  // comp returns true if the element goes before quote in preorder tree
  // traversal.
  auto comp = [](const Element& element, const LayoutQuote* quote) {
    return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
               element, *quote->GetOwningPseudo()) < 0;
  };
  // Find the first quote for which comp will return true.
  auto* it = std::upper_bound(quotes_.begin(), quotes_.end(), element, comp);
  // And get the previous quote as it will be the one we are searching for.
  return it == quotes_.begin() ? nullptr : *std::prev(it);
}

void StyleContainmentScope::AttachQuote(LayoutQuote& quote) {
  DCHECK(!quote.IsInScope());
  quote.SetScope(this);
  // Find previous in preorder quote from the current scope.
  // Don't search outside the scope subtree.
  LayoutObject* stay_within =
      GetElement() ? GetElement()->GetLayoutObject() : nullptr;
  LayoutObject* it = quote.PreviousInPreOrder(stay_within);
  for (; it; it = it->PreviousInPreOrder(stay_within)) {
    if (auto* pre_quote = DynamicTo<LayoutQuote>(it)) {
      if (pre_quote->IsInScope() && pre_quote->GetScope() == this) {
        break;
      }
    }
  }
  auto* pre_quote = DynamicTo<LayoutQuote>(it);
  // Insert at 0 if we are the new head.
  wtf_size_t pos = pre_quote ? quotes_.Find(pre_quote) + 1u : 0u;
  quotes_.insert(pos, &quote);
}

void StyleContainmentScope::DetachQuote(LayoutQuote& quote) {
  if (!quote.IsInScope()) {
    return;
  }
  wtf_size_t pos = quotes_.Find(&quote);
  DCHECK_NE(pos, kNotFound);
  quotes_.EraseAt(pos);
  quote.SetScope(nullptr);
}

int StyleContainmentScope::ComputeInitialQuoteDepth() const {
  int depth = 0;
  // Compute the depth of the previous quote from one of the parents.
  // Depth will be 0, if we are the first quote.
  for (StyleContainmentScope* parent = parent_; parent;
       parent = parent->Parent()) {
    const LayoutQuote* parent_quote =
        parent->FindQuotePrecedingElement(*quotes_.front()->GetOwningPseudo());
    if (parent_quote) {
      depth = parent_quote->GetNextDepth();
    }
  }
  return depth;
}

void StyleContainmentScope::UpdateQuotes() const {
  bool needs_children_update = false;
  if (quotes_.size()) {
    int depth = ComputeInitialQuoteDepth();
    for (LayoutQuote* quote : quotes_) {
      if (depth != quote->GetDepth()) {
        needs_children_update = true;
      }
      quote->SetDepth(depth);
      quote->UpdateText();
      depth = quote->GetNextDepth();
    }
  }
  // If nothing has changed on this level, don't update children.
  if (needs_children_update) {
    for (StyleContainmentScope* child : Children()) {
      child->UpdateQuotes();
    }
  }
}

}  // namespace blink
