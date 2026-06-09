/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_QUERY_H_

#include <iosfwd>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSSelector;
class ContainerNode;
class Document;
class ExceptionState;
class SelectorChecker;
template <typename NodeType>
class StaticNodeTypeList;
using StaticElementList = StaticNodeTypeList<Element>;

// SelectorQuery implements querySelector() and querySelectorAll()
// (which are very similar except for the stopping condition).
//
// querySelector() is the opposite problem of normal selector matching;
// selector matching generally tries to match one element to many selectors,
// while querySelector() matches one selector to a tree of many elements.
// Thus, the general working (except for weird cases like a comma-separated
// list of selectors) is also roughly the opposite of SelectorChecker:
// We start at the root of the tree and the first compound in the selector,
// and go down to and to the right in the tree as we try to match compounds
// successively to elements. (In a sense, the compounds are the states
// of a deterministic finite automaton (DFA), although we often follow
// more than one edge because we may have both children and siblings.)
// So e.g., for a DOM like this:
//
// <html>
//   <body>
//     <div class="a"></div>
//     <div class="b"></div>
//     <div class="c"></div>
//   </body>
// </html>
//
// If we have a selector like “body .a ~ .c”, we will first look down the
// tree for <body>, and only when we find it will we switch to checking for
// .a. Once we've found .a, we'll switch to looking for .c (and if we went
// down to children of .b, we'd go back to .a), and once we find that .c,
// we have our match with no further checking. (There's a complication
// in that if querySelector was called at e.g. the .a element and not at
// the entire document, we have to go up and see that “body” is indeed
// already in our ancestor chain, and discard that as already matched.
// See RemoveCompoundsSeenBeforeRoot()).
//
// We use the Bloom subtree filters stored in the DOM at every step,
// to prune out impossible branches of the DOM quickly without looking
// at them at all. For instance, for a selector like “.d” on the given DOM,
// we'd look at only the root element and then discard the entire tree
// immediately (unless there's a false positive in the Bloom filter).
//
// In order to not have to reimplement all of SelectorChecker, we only support
// checking some common selectors, namely ID, class, tag/universal,
// exact attribute and :nth-child(); everything else is just assumed to match.
// Also, we relax the combinators so that > becomes the descendant combinator
// and + becomes ~; this allows us to do a greedy match without any backtracking
// in the DOM. When we do these relaxations, we re-check with SelectorChecker
// in order to not have false positives (it is skipped otherwise, for speed).
//
// Furthermore, in standards-mode documents, we can accelerate ID selectors
// (#foo, and their weird cousin [id=foo]) by using the precalculated
// getElementById() data in the DOM to skip directly to the element(s)
// with the given ID. This holds whether the ID selector is in the subject
// or further up. For instance, assuming this DOM:
//
// <html>
//   <body>
//     <div class="a"></div>
//     <div id="id"></div>
//     <div class="b"></div>
//   </body>
// </html>
//
// If we have a selector like “body #id ~ .b”, we'd find the #id element
// in O(1), discard the body part of the selector (we could recheck it,
// but we currently don't) and start looking for a sibling matching .b.
// If we don't find one, we immediately stop in this case -- we don't
// look for children below the #id element, only .b.
class CORE_EXPORT SelectorQuery : public GarbageCollected<SelectorQuery> {
 public:
  explicit SelectorQuery(CSSSelectorList*);
  SelectorQuery(const SelectorQuery&) = delete;
  SelectorQuery& operator=(const SelectorQuery&) = delete;

  // https://dom.spec.whatwg.org/#dom-element-matches
  bool Matches(Element&) const;

  // https://dom.spec.whatwg.org/#dom-element-closest
  Element* Closest(Element&) const;

  // https://dom.spec.whatwg.org/#dom-parentnode-queryselectorall
  StaticElementList* QueryAll(ContainerNode& root_node) const;

  // https://dom.spec.whatwg.org/#dom-parentnode-queryselector
  Element* QueryFirst(ContainerNode& root_node) const;

  struct QueryStats {
    unsigned elements_seen = 0;
    unsigned fast_id_roots = 0;
    unsigned check_id = 0;
    unsigned check_tag = 0;
    unsigned check_class = 0;
    unsigned check_attr = 0;
    unsigned check_nth_child = 0;
    unsigned recheck_selector = 0;
    unsigned skipped_subtree = 0;
    unsigned slow_scan = 0;

    bool operator==(const QueryStats& other) const {
      return elements_seen == other.elements_seen &&
             fast_id_roots == other.fast_id_roots &&
             check_id == other.check_id && check_class == other.check_class &&
             check_tag == other.check_tag && check_attr == other.check_attr &&
             check_nth_child == other.check_nth_child &&
             recheck_selector == other.recheck_selector &&
             skipped_subtree == other.skipped_subtree &&
             slow_scan == other.slow_scan;
    }
  };
  // Used by unit tests to get information about what paths were taken during
  // the last query. Always reset between queries. This system is disabled in
  // non DCHECK builds to avoid the overhead on the query process.
  static QueryStats LastQueryStats();

  void Trace(Visitor* visitor) const { visitor->Trace(selector_list_); }

 private:
  // We add this flag if we don't actually know the sibling index
  // (this happens only on the top level; when we do firstChild(),
  // we know that we start at 1); it is easy to increment such
  // “invalid” values (the flag is still set on any reasonable DOM),
  // and it's a bit tighter than a full std::optional<unsigned>.
  static constexpr unsigned kUnknownSiblingIndex = 0x80000000;

  // Extracts the features of a single compound selector (i.e., a combination
  // of simple selectors that match the same element), and also equivalent
  // to a single state in our state machine.
  //
  // We store information that is useful to quickly reject the selector
  // for a given element on this compound (or, ideally, match it and skip
  // full selector matching entirely), information for when to actually
  // end the match, and how we progress through the states. Every matching
  // field may be unset; e.g., for a compound like div#id, the class_needed
  // field will be unset as there was no class selector.
  //
  // Often, there will only be a single compound, e.g. for selectors
  // such as #id or div.cls.
  struct Compound {
    AtomicString id_needed;
    QualifiedName tag_needed = QualifiedName::Null();
    AtomicString class_needed;

    // These six are set together or not at all; they represent an
    // exact attribute selector (i.e., [foo=bar]).
    QualifiedName attr_needed = QualifiedName::Null();
    AtomicString attr_value;
    // Whether attr_value is matched case-insensitively or not.
    // Filled in only once we know the document.
    mutable bool attr_case_insensitive;
    mutable bool needs_synchronize_attribute;  // Same.
    bool match_type_case_insensitive;          // Used to calculate
                                               // attr_case_insensitive.
    bool legacy_case_insensitive;  // Used to calculate attr_case_insensitive.

    unsigned nth_child = 0;  // If 0, no :nth-child() will be tested.

    // Used to quickly reject large parse of the tree; if an element
    // doesn't contain this in its subtree Bloom filter, we can stop
    // matching altogether.
    //
    // If the next compound is vertical (i.e., not a + or ~ combinator),
    // this will include everything below the element in the DOM.
    // E.g., for .a + .b > .c + .d, the compound for .a will contain
    // a selector filter for only .a, but the one for .b will contain
    // all of .b.c.d.
    Element::TinyBloomFilter selector_filter{0};

    // For a selector like :host .a with an element directly under the shadow
    // root, we need the ShadowRoot to “match” :host or we won't progress
    // appropriately; we don't traverse across shadow trees, and we don't
    // actually go and find the actual host to match (it would be featureless
    // anyway). So for the cases where there are no other selectors in the
    // compound, we allow that to match any Element or the ShadowRoot (and then,
    // of course, we need to run full selector matching afterwards).
    // In other words, this is only to avoid false negatives.
    bool skip_for_shadow_root = true;

    // Whether the combinator from this compound is a sibling combinator
    // (+ or ~), i.e., after matching this, we need to look to the right
    // in the tree to progress through the selector, instead of downwards.
    //
    // Not used for last compound (the subject).
    bool next_compound_is_horizontal = false;

    // Whether this is the last compound (if this compound matches,
    // we go to full selector checking if needed and then record
    // the match).
    bool is_subject = false;

    // If true, we can never escape from this state; it is the subject
    // and it isn't matched against any siblings (so it cannot go backwards
    // even on a mismatch). In this case, we follow a more traditional
    // recursion-free traversal of the remaining subtree, since this works
    // somewhat faster in extreme cases. (For more typical real-world cases,
    // it's better in some cases and worse in others; following parent pointers
    // has a cost, too.)
    bool simple_traversal_from_here = false;

    // If this is false, we never allow progressing into this compound (state);
    // we can start there, but going back to it (or just into it) will
    // immediately stop traversal. This is used when we anchor the search on an
    // #id match; we know that the start element must be matched against the
    // given compound, and thus don't search the entire subtree on a non-match.
    // E.g., for a selector like .a > .b ~ #id ~ .c > .d, only .c and .d are
    // marked as valid_for_progress; since children of #id (and in general, any
    // sibling that doesn't match .c) would always go back to .b, they are not
    // considered at all. (The flag for .a does not matter, since we can never
    // go back to that state anyway.) Similarly, if we have a selector such as
    // div#id > .a, the first state would be marked as valid_for_progress=false,
    // and if we don't match it on the first element, we'll stop immediately.
    // Even if we _do_ match div#id, we wouldn't consider any siblings of the
    // #id element, as they'd stay in the same state.
    //
    // For non-ID searches, all compounds are marked as valid.
    mutable bool valid_for_progress = true;

    // Used internally in RemoveCompoundsSeenBeforeRoot(), so only during
    // actual matching, and will be frequently overwritten.
    mutable bool seen_before_root;

    // The next compound (state) to go to after this compound, where
    // the input is a combination of:
    //
    //   a) is the next element we're interested in a child or a sibling,
    //      and
    //   b) did we actually match the compound or not.
    //
    // We will often follow two edges (if we have both a child or a sibling),
    // which makes the automaton not strictly deterministic.
    //
    // We do not store an explicit edge for (sibling, mismatch), since it's
    // always the same compound.
    const Compound* next_compound_for_children_on_match = nullptr;
    const Compound* next_compound_for_siblings_on_match = nullptr;
    const Compound* next_compound_for_children_on_mismatch = nullptr;
  };

  // See if we have compounds above our root that we must skip;
  // e.g., if we have a selector like “.foo > .bar .baz”, we would like to
  // first search for .foo and then search only under those, but if
  // we actually have a .foo above our root, we need to go directly
  // to looking for .bar (which makes for a less restrictive and thus
  // slower search). If we have both .foo and .bar above our root,
  // we need to look directly for .baz. The same goes for compounds
  // to the left of the root (horizontally=true), once we've discovered
  // what is our top level. Returns the index of the first unmatched
  // compound (ie., where to start).
  //
  // For simplicity, we don't check that they actually come in a given
  // structure or order, or even that a single element isn't used for
  // all of the compounds. Any false positives here will only make us
  // slower, not incorrect. (The same goes for selectors that require
  // full checking.)
  unsigned FirstCompoundNotSeenBeforeRoot(const ContainerNode& root_node,
                                          unsigned from_idx,
                                          unsigned to_idx,
                                          bool is_html_doc,
                                          bool horizontally) const;

  unsigned FindStartOfLevel(unsigned compound_idx) const;

  template <typename SelectorQueryTrait>
  void ExecuteSlow(ContainerNode& root_node,
                   typename SelectorQueryTrait::OutputType&) const;
  template <typename SelectorQueryTrait>
  void Execute(ContainerNode& root_node,
               typename SelectorQueryTrait::OutputType&) const;

  bool SelectorListMatches(ContainerNode& root_node, Element&) const;

  // Points to the start of the single usable selector.
  // Note that this may be different from selector_list_->First(),
  // if we had an invalid selector first that we could filter out
  // entirely.
  const CSSSelector* OnlySelector() const {
    CHECK_EQ(1u, selector_start_offsets_.size());
    return UNSAFE_TODO(selector_list_->First() + selector_start_offsets_[0]);
  }

  void BuildCompounds(const CSSSelector* selector);

  // Returns true if the search is done (because we found an element
  // and are only looking for one).
  template <typename SelectorQueryTrait, bool is_top_level = true>
  bool ExecuteSearch(ContainerNode& node,
                     const ContainerNode& scope,
                     const Compound* compound,
                     unsigned sibling_idx,
                     bool need_full_check,
                     bool is_html_doc,
                     SelectorChecker& checker,
                     typename SelectorQueryTrait::OutputType& output) const;

  // Search a subtree, but limited to a single compound (no state changes)
  // and using parent pointers instead of recursing. See
  // simple_traversal_from_here.
  template <typename SelectorQueryTrait>
  ALWAYS_INLINE bool ExecuteSearchSingleCompound(
      ContainerNode& root_node,
      Element& first_node,
      const ContainerNode& scope,
      const Compound* compound,
      bool need_full_check,
      bool is_html_doc,
      SelectorChecker& checker,
      typename SelectorQueryTrait::OutputType& output) const;

  ALWAYS_INLINE static bool MatchCompound(const Element& element,
                                          const Compound& compound,
                                          unsigned sibling_idx,
                                          bool is_html_doc);

  void FillMissingData(const ContainerNode& root_node) const;

  Member<CSSSelectorList> selector_list_;
  // Contains the start of each complex selector (relative to the first selector
  // in selector_list_; we cannot store pointers due to Oilpan restrictions),
  // but without ones that could never match like pseudo-elements, div::before.
  // This can be empty, while |selector_list_| will never be empty, as
  // SelectorQueryCache::add would have thrown an exception.
  Vector<unsigned, 4> selector_start_offsets_;

  // If we have only a single complex selector, this contains
  // pre-parsed Compounds for it, to use in fast matching.
  // Otherwise empty.
  Vector<Compound, 2> compounds_;

  // If true, the selector contains selectors not covered by need_full_check_,
  // and we need to do a full selector check in the end. (Even if false,
  // runtime situations may necessitate that we do such a check anyway.)
  // Only relevant if we have exactly one complex selector.
  bool need_full_check_;

  // If there is a compound with an #id or [id=foo] selector, list its index.
  // This is used for the getElementById() fast path, as described in the
  // class comment.
  //
  // Only relevant if we have exactly one complex selector.
  int last_compound_with_id_selector_ = -1;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const SelectorQuery::QueryStats&);

class SelectorQueryCache : public GarbageCollected<SelectorQueryCache> {
 public:
  SelectorQuery* Add(const AtomicString&, const Document&, ExceptionState&);
  void Invalidate();

  void Trace(Visitor* visitor) const { visitor->Trace(entries_); }

 private:
  HeapHashMap<AtomicString, Member<SelectorQuery>> entries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_QUERY_H_
