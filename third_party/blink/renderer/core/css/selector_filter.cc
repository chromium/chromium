/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/selector_filter.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"

namespace blink {

namespace {

// Salt to separate otherwise identical string hashes so a class-selector like
// .article won't match <article> elements.
enum { kTagNameSalt = 13, kIdSalt = 17, kClassSalt = 19, kAttributeSalt = 23 };

inline bool IsExcludedAttribute(const AtomicString& name) {
  return name == html_names::kClassAttr.LocalName() ||
         name == html_names::kIdAttr.LocalName() ||
         name == html_names::kStyleAttr.LocalName();
}

inline void CollectElementIdentifierHashes(
    const Element& element,
    Vector<unsigned, 4>& identifier_hashes) {
  identifier_hashes.push_back(
      element.LocalNameForSelectorMatching().Impl()->ExistingHash() *
      kTagNameSalt);
  if (element.HasID()) {
    identifier_hashes.push_back(
        element.IdForStyleResolution().Impl()->ExistingHash() * kIdSalt);
  }

  if (element.IsStyledElement() && element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    wtf_size_t count = class_names.size();
    for (wtf_size_t i = 0; i < count; ++i) {
      identifier_hashes.push_back(class_names[i].Impl()->ExistingHash() *
                                  kClassSalt);
    }
  }
  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    auto attribute_name = attribute_item.LocalName();
    if (IsExcludedAttribute(attribute_name)) {
      continue;
    }
    auto lower = attribute_name.IsLowerASCII() ? attribute_name
                                               : attribute_name.LowerASCII();
    identifier_hashes.push_back(lower.Impl()->ExistingHash() * kAttributeSalt);
  }
}

void CollectDescendantCompoundSelectorIdentifierHashes(
    const CSSSelector* selector,
    CSSSelector::RelationType relation,
    unsigned*& hash,
    unsigned* end);

inline void CollectDescendantSelectorIdentifierHashes(
    const CSSSelector& selector,
    unsigned*& hash,
    unsigned* end) {
  switch (selector.Match()) {
    case CSSSelector::kId:
      if (!selector.Value().empty()) {
        (*hash++) = selector.Value().Impl()->ExistingHash() * kIdSalt;
      }
      break;
    case CSSSelector::kClass:
      if (!selector.Value().empty()) {
        (*hash++) = selector.Value().Impl()->ExistingHash() * kClassSalt;
      }
      break;
    case CSSSelector::kTag:
      if (selector.TagQName().LocalName() !=
          CSSSelector::UniversalSelectorAtom()) {
        (*hash++) = selector.TagQName().LocalName().Impl()->ExistingHash() *
                    kTagNameSalt;
      }
      break;
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
    case CSSSelector::kAttributeHyphen: {
      auto attribute_name = selector.Attribute().LocalName();
      if (IsExcludedAttribute(attribute_name)) {
        break;
      }
      auto lower_name = attribute_name.IsLowerASCII()
                            ? attribute_name
                            : attribute_name.LowerASCII();
      (*hash++) = lower_name.Impl()->ExistingHash() * kAttributeSalt;
    } break;
    case CSSSelector::kPseudoClass:
      switch (selector.GetPseudoType()) {
        case CSSSelector::kPseudoIs:
        case CSSSelector::kPseudoWhere:
        case CSSSelector::kPseudoParent: {
          // If we have a one-element :is(), :where() or &, treat it
          // as if the given list was written out as a normal descendant.
          const CSSSelector* selector_list = selector.SelectorListOrParent();
          if (selector_list &&
              CSSSelectorList::Next(*selector_list) == nullptr) {
            CollectDescendantCompoundSelectorIdentifierHashes(
                selector_list, CSSSelector::kDescendant, hash, end);
          }
          break;
        }
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void CollectDescendantCompoundSelectorIdentifierHashes(
    const CSSSelector* selector,
    CSSSelector::RelationType relation,
    unsigned*& hash,
    unsigned* end) {
  // Skip the rightmost compound. It is handled quickly by the rule hashes.
  bool skip_over_subselectors = true;
  for (const CSSSelector* current = selector; current;
       current = current->TagHistory()) {
    // Only collect identifiers that match ancestors.
    switch (relation) {
      case CSSSelector::kSubSelector:
      case CSSSelector::kScopeActivation:
        if (!skip_over_subselectors) {
          CollectDescendantSelectorIdentifierHashes(*current, hash, end);
        }
        break;
      case CSSSelector::kDirectAdjacent:
      case CSSSelector::kIndirectAdjacent:
        skip_over_subselectors = true;
        break;
      case CSSSelector::kShadowSlot:
      case CSSSelector::kDescendant:
      case CSSSelector::kChild:
      case CSSSelector::kUAShadow:
      case CSSSelector::kShadowPart:
        skip_over_subselectors = false;
        CollectDescendantSelectorIdentifierHashes(*current, hash, end);
        break;
      case CSSSelector::kRelativeDescendant:
      case CSSSelector::kRelativeChild:
      case CSSSelector::kRelativeDirectAdjacent:
      case CSSSelector::kRelativeIndirectAdjacent:
        NOTREACHED();
        break;
    }
    if (hash == end) {
      return;
    }
    relation = current->Relation();
  }
}

}  // namespace

void SelectorFilter::PushParentStackFrame(Element& parent) {
  DCHECK(ancestor_identifier_filter_);
  DCHECK(parent_stack_.empty() || parent_stack_.back().element ==
                                      FlatTreeTraversal::ParentElement(parent));
  DCHECK(!parent_stack_.empty() || !FlatTreeTraversal::ParentElement(parent));
  parent_stack_.push_back(ParentStackFrame(parent));
  ParentStackFrame& parent_frame = parent_stack_.back();
  // Mix tags, class names and ids into some sort of weird bouillabaisse.
  // The filter is used for fast rejection of child and descendant selectors.
  CollectElementIdentifierHashes(parent, parent_frame.identifier_hashes);
  wtf_size_t count = parent_frame.identifier_hashes.size();
  for (wtf_size_t i = 0; i < count; ++i) {
    ancestor_identifier_filter_->Add(parent_frame.identifier_hashes[i]);
  }
}

void SelectorFilter::PopParentStackFrame() {
  DCHECK(!parent_stack_.empty());
  DCHECK(ancestor_identifier_filter_);
  const ParentStackFrame& parent_frame = parent_stack_.back();
  wtf_size_t count = parent_frame.identifier_hashes.size();
  for (wtf_size_t i = 0; i < count; ++i) {
    ancestor_identifier_filter_->Remove(parent_frame.identifier_hashes[i]);
  }
  parent_stack_.pop_back();
  if (parent_stack_.empty()) {
#if DCHECK_IS_ON()
    DCHECK(ancestor_identifier_filter_->LikelyEmpty());
#endif
    ancestor_identifier_filter_.reset();
  }
}

void SelectorFilter::PushParent(Element& parent) {
  DCHECK(parent.GetDocument().InStyleRecalc());
  DCHECK(parent.InActiveDocument());
  if (parent_stack_.empty()) {
    DCHECK_EQ(parent, parent.GetDocument().documentElement());
    DCHECK(!ancestor_identifier_filter_);
    ancestor_identifier_filter_ = std::make_unique<IdentifierFilter>();
    PushParentStackFrame(parent);
    return;
  }
  DCHECK(ancestor_identifier_filter_);
  // We may get invoked for some random elements in some wacky cases during
  // style resolve. Pause maintaining the stack in this case.
  if (parent_stack_.back().element !=
      FlatTreeTraversal::ParentElement(parent)) {
    return;
  }
  PushParentStackFrame(parent);
}

void SelectorFilter::PopParent(Element& parent) {
  DCHECK(parent.GetDocument().InStyleRecalc());
  DCHECK(parent.InActiveDocument());
  // Note that we may get invoked for some random elements in some wacky cases
  // during style resolve. Pause maintaining the stack in this case.
  if (!ParentStackIsConsistent(&parent)) {
    return;
  }
  PopParentStackFrame();
}

void SelectorFilter::CollectIdentifierHashes(
    const CSSSelector& selector,
    unsigned* identifier_hashes,
    unsigned maximum_identifier_count) {
  unsigned* hash = identifier_hashes;
  unsigned* end = identifier_hashes + maximum_identifier_count;

  CollectDescendantCompoundSelectorIdentifierHashes(
      selector.TagHistory(), selector.Relation(), hash, end);
  if (hash != end) {
    *hash = 0;
  }
}

void SelectorFilter::ParentStackFrame::Trace(Visitor* visitor) const {
  visitor->Trace(element);
}

void SelectorFilter::Trace(Visitor* visitor) const {
  visitor->Trace(parent_stack_);
}

}  // namespace blink
