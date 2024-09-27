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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_FILTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSSelector;
class StyleScope;

// SelectorFilter is a bloom filter for rapidly discarding style rules that
// have ancestor requirements. When we traverse the DOM, we call PushParent()
// for each parent, which inserts a number of relevant properties for that
// parent (e.g. ID, tag name, attributes etc.) into the filter. (We also call
// PopParent() when exiting a node, which is possible because the filter is
// a counting filter.) Then, when we want to match a style rule with at least
// one such ancestor attribute, we can very cheaply check whether an ancestor
// exists in the filter (with some false positives, but that's fine).
// For instance, assume this tree:
//
//   <div id="a" data-foo="bar">
//     <div class="cls">
//       <div id="b">
//
// When we get to computing style for the innermost element, the bloom filter
// will contain hashes corresponding to <div> (twice), [data-foo], #a and .cls.
// If we then have a rule saying e.g. “article #b”, we can look up <article> in
// the bloom filter and get a negative result (save for false positives),
// proving that the rule definitely does not apply, discarding it right away.
// However, a rule like “.cls[data-foo] #b” would pass the filter, as there are
// indeed hashes for both .cls and [data-foo] in the filter. Thus, any rule
// passing the filter must still be subjected to match checking as usual.
//
// For performance reasons, we compute the ancestor hash values for each style
// rule ahead-of-time. We stop after at most four hashes to avoid allocating
// memory dynamically, but elements (represented by ParentStackFrame) cannot
// have such a limit, or we would risk false negatives, causing us to miss
// applicable style rules in matching.
//
// For practical web pages as of 2022, we've seen SelectorFilter discard 60-70%
// of rules in early processing, which makes the 4 kB of RAM/cache it uses
// worthwhile.
class CORE_EXPORT SelectorFilter {
  DISALLOW_NEW();

 public:
  SelectorFilter() = default;
  SelectorFilter(const SelectorFilter&) = delete;
  SelectorFilter& operator=(const SelectorFilter&) = delete;

  // Call before the first PushParent(), if you are starting traversal at
  // some tree scope that is not at the root of the document.
  void PushAllParentsOf(TreeScope& tree_scope);

  void PushParent(Element& parent);
  void PopParent(Element& parent);

  bool ParentStackIsConsistent(const Element* parent) const {
    if (parent == nullptr) {
      return parent_stack_.empty();
    } else {
      return !parent_stack_.empty() && parent_stack_.back() == parent;
    }
  }

  inline bool FastRejectSelector(
      const base::span<const unsigned> identifier_hashes) const;
  static void CollectIdentifierHashes(const CSSSelector&,
                                      const StyleScope*,
                                      Vector<unsigned>& bloom_hash_backing);

  void Trace(Visitor*) const;

 private:
  void PushAncestors(const Node& node);
  void PushParentStackFrame(Element& parent);
  void PopParentStackFrame();

  HeapVector<Member<Element>> parent_stack_;

  // With 100 unique strings in the filter, 2^12 slot table has false positive
  // rate of ~0.2%.
  using IdentifierFilter = CountingBloomFilter<12>;
  IdentifierFilter ancestor_identifier_filter_;
};

inline bool SelectorFilter::FastRejectSelector(
    const base::span<const unsigned> identifier_hashes) const {
  for (unsigned hash : identifier_hashes) {
    if (!ancestor_identifier_filter_.MayContain(hash)) {
      return true;
    }
  }
  return false;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_FILTER_H_
