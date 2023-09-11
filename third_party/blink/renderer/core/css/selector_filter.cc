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
#include "third_party/blink/renderer/core/css/style_scope.h"
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

template <class Func>
inline void CollectElementIdentifierHashes(const Element& element,
                                           Func&& func) {
  func(element.LocalNameForSelectorMatching().Hash() * kTagNameSalt);
  if (element.HasID()) {
    func(element.IdForStyleResolution().Hash() * kIdSalt);
  }

  if (element.IsStyledElement() && element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    wtf_size_t count = class_names.size();
    for (wtf_size_t i = 0; i < count; ++i) {
      func(class_names[i].Hash() * kClassSalt);
    }
  }
  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    const AtomicString& attribute_name = attribute_item.LocalName();
    if (IsExcludedAttribute(attribute_name)) {
      continue;
    }
    if (attribute_name.IsLowerASCII()) {
      func(attribute_name.Hash() * kAttributeSalt);
    } else {
      func(attribute_name.LowerASCII().Hash() * kAttributeSalt);
    }
  }
}

void CollectDescendantCompoundSelectorIdentifierHashes(
    const CSSSelector* selector,
    CSSSelector::RelationType relation,
    const StyleScope* style_scope,
    unsigned*& hash,
    unsigned* end);

inline void CollectDescendantSelectorIdentifierHashes(
    const CSSSelector& selector,
    const StyleScope* style_scope,
    unsigned*& hash,
    unsigned* end) {
  switch (selector.Match()) {
    case CSSSelector::kId:
      if (!selector.Value().empty()) {
        (*hash++) = selector.Value().Hash() * kIdSalt;
      }
      break;
    case CSSSelector::kClass:
      if (!selector.Value().empty()) {
        (*hash++) = selector.Value().Hash() * kClassSalt;
      }
      break;
    case CSSSelector::kTag:
      if (selector.TagQName().LocalName() !=
          CSSSelector::UniversalSelectorAtom()) {
        (*hash++) = selector.TagQName().LocalName().Hash() * kTagNameSalt;
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
      (*hash++) = lower_name.Hash() * kAttributeSalt;
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
                selector_list, CSSSelector::kDescendant, style_scope, hash,
                end);
          }
          break;
        }
        case CSSSelector::kPseudoScope:
          if (style_scope) {
            const CSSSelector* selector_list = style_scope->From();
            if (selector_list &&
                CSSSelectorList::Next(*selector_list) == nullptr) {
              CollectDescendantCompoundSelectorIdentifierHashes(
                  selector_list, CSSSelector::kDescendant,
                  style_scope->Parent(), hash, end);
            }
          }
          break;
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
    const StyleScope* style_scope,
    unsigned*& hash,
    unsigned* end) {
  // Skip the rightmost compound. It is handled quickly by the rule hashes.
  bool skip_over_subselectors = true;
  for (const CSSSelector* current = selector; current;
       current = current->NextSimpleSelector()) {
    // Only collect identifiers that match ancestors.
    switch (relation) {
      case CSSSelector::kSubSelector:
      case CSSSelector::kScopeActivation:
        if (!skip_over_subselectors) {
          CollectDescendantSelectorIdentifierHashes(*current, style_scope, hash,
                                                    end);
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
        CollectDescendantSelectorIdentifierHashes(*current, style_scope, hash,
                                                  end);
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
  DCHECK(parent_stack_.empty() ||
         parent_stack_.back() == FlatTreeTraversal::ParentElement(parent));
  DCHECK(!parent_stack_.empty() || !FlatTreeTraversal::ParentElement(parent));
  parent_stack_.push_back(parent);
  // Mix tags, class names and ids into some sort of weird bouillabaisse.
  // The filter is used for fast rejection of child and descendant selectors.
  CollectElementIdentifierHashes(parent, [this](unsigned hash) {
    ancestor_identifier_filter_->Add(hash);
  });
}

void SelectorFilter::PopParentStackFrame() {
  DCHECK(!parent_stack_.empty());
  DCHECK(ancestor_identifier_filter_);
  CollectElementIdentifierHashes(*parent_stack_.back(), [this](unsigned hash) {
    ancestor_identifier_filter_->Remove(hash);
  });
  parent_stack_.pop_back();
  if (parent_stack_.empty()) {
#if DCHECK_IS_ON()
    DCHECK(ancestor_identifier_filter_->LikelyEmpty());
#endif
    ancestor_identifier_filter_.reset();
  }
}

void SelectorFilter::PushAllParentsOf(TreeScope& tree_scope) {
  PushAncestors(tree_scope.RootNode());
}

void SelectorFilter::PushAncestors(const Node& node) {
  Element* parent = node.ParentOrShadowHostElement();
  if (parent != nullptr) {
    PushAncestors(*parent);
    PushParent(*parent);
  }
}

void SelectorFilter::PushParent(Element& parent) {
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
  if (parent_stack_.back() != FlatTreeTraversal::ParentElement(parent)) {
    return;
  }
  PushParentStackFrame(parent);
}

void SelectorFilter::PopParent(Element& parent) {
  // Note that we may get invoked for some random elements in some wacky cases
  // during style resolve. Pause maintaining the stack in this case.
  if (!ParentStackIsConsistent(&parent)) {
    return;
  }
  PopParentStackFrame();
}

void SelectorFilter::CollectIdentifierHashes(
    const CSSSelector& selector,
    const StyleScope* style_scope,
    unsigned* identifier_hashes,
    unsigned maximum_identifier_count) {
  unsigned* hash = identifier_hashes;
  unsigned* end = identifier_hashes + maximum_identifier_count;

  CollectDescendantCompoundSelectorIdentifierHashes(
      selector.NextSimpleSelector(), selector.Relation(), style_scope, hash,
      end);
  if (hash != end) {
    *hash = 0;
  }
}

void SelectorFilter::Trace(Visitor* visitor) const {
  visitor->Trace(parent_stack_);
}

}  // namespace blink
