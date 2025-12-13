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
enum { kTagNameSalt = 1, kIdSalt = 3, kClassSalt = 5, kAttributeSalt = 7 };

template <class Func>
inline void CollectElementIdentifierHashes(const Element& element,
                                           Func&& func) {
  func(element.LocalNameForSelectorMatching().Hash() * kTagNameSalt);
  if (element.HasID()) {
    func(element.IdForStyleResolution().Hash() * kIdSalt);
  }

  if (element.IsStyledElement() && element.HasClass()) {
    for (const AtomicString& class_name : element.ClassNames()) {
      func(class_name.Hash() * kClassSalt);
    }
  }
  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    const AtomicString& attribute_name = attribute_item.LocalName();
    if (Element::IsExcludedAttribute(attribute_item.GetName(),
                                     Element::kExcludeStandardAttributesOnly)) {
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
    Vector<uint16_t>& hashes);

inline void CollectDescendantSelectorIdentifierHashes(
    const CSSSelector& selector,
    const StyleScope* style_scope,
    Vector<uint16_t>& hashes) {
  switch (selector.Match()) {
    case CSSSelector::kId:
      if (!selector.Value().empty()) {
        hashes.push_back(selector.Value().Hash() * kIdSalt);
      }
      break;
    case CSSSelector::kClass:
      if (!selector.Value().empty()) {
        hashes.push_back(selector.Value().Hash() * kClassSalt);
      }
      break;
    case CSSSelector::kTag:
      hashes.push_back(selector.TagQName().LocalName().Hash() * kTagNameSalt);
      break;
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
    case CSSSelector::kAttributeHyphen: {
      if (Element::IsExcludedAttribute(
              selector.Attribute(), Element::kExcludeStandardAttributesOnly)) {
        break;
      }
      const AtomicString& attribute_name = selector.Attribute().LocalName();
      auto lower_name = attribute_name.IsLowerASCII()
                            ? attribute_name
                            : attribute_name.LowerASCII();
      hashes.push_back(lower_name.Hash() * kAttributeSalt);
    } break;
    case CSSSelector::kPseudoClass:
      switch (selector.GetPseudoType()) {
        case CSSSelector::kPseudoIs:
        case CSSSelector::kPseudoWhere:
        case CSSSelector::kPseudoParent: {
          // If we have a one-element :is(), :where() or &, treat it
          // as if the given list was written out as a normal descendant.
          //
          // TODO: Consider whether we can do the same here as for subject
          // filters further down, so that e.g. :is(.a.b, .c.a) would at least
          // add the hash for .a.
          const CSSSelector* selector_list = selector.SelectorListOrParent();
          if (selector_list &&
              CSSSelectorList::Next(*selector_list) == nullptr) {
            CollectDescendantCompoundSelectorIdentifierHashes(
                selector_list, CSSSelector::kDescendant, style_scope, hashes);
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
                  style_scope->Parent(), hashes);
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
    Vector<uint16_t>& hashes) {
  // Skip the rightmost compound. It is handled quickly by the rule hashes.
  bool skip_over_subselectors = true;
  for (const CSSSelector* current = selector; current;
       current = current->NextSimpleSelector()) {
    // Only collect identifiers that match ancestors.
    switch (relation) {
      case CSSSelector::kSubSelector:
        if (!skip_over_subselectors) {
          CollectDescendantSelectorIdentifierHashes(*current, style_scope,
                                                    hashes);
        }
        break;
      case CSSSelector::kDirectAdjacent:
      case CSSSelector::kIndirectAdjacent:
      case CSSSelector::kPseudoChild:
        skip_over_subselectors = true;
        break;
      case CSSSelector::kShadowSlot:
      case CSSSelector::kDescendant:
      case CSSSelector::kChild:
      case CSSSelector::kUAShadow:
      case CSSSelector::kShadowPart:
        skip_over_subselectors = false;
        CollectDescendantSelectorIdentifierHashes(*current, style_scope,
                                                  hashes);
        break;
      case CSSSelector::kRelativeDescendant:
      case CSSSelector::kRelativeChild:
      case CSSSelector::kRelativeDirectAdjacent:
      case CSSSelector::kRelativeIndirectAdjacent:
        NOTREACHED();
    }
    relation = current->Relation();
  }
}

}  // namespace

void SelectorFilter::CollectSubjectIdentifierHashes(
    const CSSSelector* selector,
    Element::AttributesToExcludeHashesFor attributes_to_exclude,
    Element::TinyBloomFilter& subject_filter) {
  for (const CSSSelector* current = selector; current;
       current = current->NextSimpleSelector()) {
    switch (current->Match()) {
      case CSSSelector::kClass:
        if (!current->Value().empty()) {
          subject_filter |= Element::FilterForString(current->Value());
        }
        break;
      case CSSSelector::kAttributeExact:
      case CSSSelector::kAttributeSet:
      case CSSSelector::kAttributeList:
      case CSSSelector::kAttributeContain:
      case CSSSelector::kAttributeBegin:
      case CSSSelector::kAttributeEnd:
      case CSSSelector::kAttributeHyphen: {
        if (Element::IsExcludedAttribute(current->Attribute(),
                                         attributes_to_exclude)) {
          break;
        }
        subject_filter |= Element::FilterForAttribute(current->Attribute());
        break;
      }
      case CSSSelector::kPseudoClass:
        switch (current->GetPseudoType()) {
          case CSSSelector::kPseudoIs:
          case CSSSelector::kPseudoWhere:
          case CSSSelector::kPseudoParent: {
            // If we have a :is(), :where() or &, and all alternatives share
            // one or more bits (for instance because there is only one
            // alternative), we can require those bits.
            //
            // If the list is empty, this ends up requiring all bits, which is
            // fine (since :is() can never match anything anyway). The exception
            // is if an empty list signifies parent-for-scope.
            if (current->GetPseudoType() == CSSSelector::kPseudoParent &&
                !current->SelectorListOrParent()) {
              // & for @scope (as opposed to & for nesting). We don't know
              // what this ends up pointing to, so we also cannot add
              // anything to the filter.
            } else {
              Element::TinyBloomFilter intersection =
                  ~Element::TinyBloomFilter{0};
              for (const CSSSelector* sub_selector =
                       current->SelectorListOrParent();
                   sub_selector;
                   sub_selector = CSSSelectorList::Next(*sub_selector)) {
                Element::TinyBloomFilter sub_filter = 0;
                CollectSubjectIdentifierHashes(
                    sub_selector, attributes_to_exclude, sub_filter);
                intersection &= sub_filter;
              }
              subject_filter |= intersection;
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

    // Don't look past the subject.
    if (current->Relation() != CSSSelector::kSubSelector) {
      break;
    }
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
#if DCHECK_IS_ON()
  if (parent_stack_.empty()) {
    DCHECK_EQ(parent, parent.GetDocument().documentElement());
  } else if (parent_stack_.back() != FlatTreeTraversal::ParentElement(parent) &&
             parent_stack_.back() != parent.ParentOrShadowHostElement()) {
    LOG(DFATAL) << "Parent stack must be consistent; pushed " << parent
                << " with parent " << parent.ParentOrShadowHostElement()
                << " and flat-tree parent "
                << FlatTreeTraversal::ParentElement(parent)
                << ", but the stack contained " << parent_stack_.back()
                << ", which is neither";
  }
#endif
  parent_stack_.push_back(parent);
  // Mix tags, class names and ids into some sort of weird bouillabaisse.
  // The filter is used for fast rejection of child and descendant selectors.
  CollectElementIdentifierHashes(parent, [this](unsigned hash) {
    hash &= kFilterMask;
    if (!ancestor_identifier_filter_.test(hash)) {
      ancestor_identifier_filter_.set(hash);
      set_bits_.push_back(hash);
    }
  });
}

void SelectorFilter::CollectIdentifierHashes(
    const CSSSelector& selector,
    const StyleScope* style_scope,
    Vector<uint16_t>& bloom_hash_backing,
    Element::TinyBloomFilter& subject_filter) {
  CollectDescendantCompoundSelectorIdentifierHashes(
      selector.NextSimpleSelector(), selector.Relation(), style_scope,
      bloom_hash_backing);
  subject_filter = 0;
  CollectSubjectIdentifierHashes(
      &selector, Element::kExcludeAllLazilySynchronizedAttributes,
      subject_filter);
}

void SelectorFilter::Trace(Visitor* visitor) const {
  visitor->Trace(parent_stack_);
}

}  // namespace blink
