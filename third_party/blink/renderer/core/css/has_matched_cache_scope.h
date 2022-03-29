// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_MATCHED_CACHE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_MATCHED_CACHE_SCOPE_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class CSSSelector;
class Document;
class HasArgumentMatchContext;

// To determine whether a :has() pseudo class matches an element or not, we need
// to match the :has() argument selector to the descendants, next siblings or
// next sibling descendants. While matching the :has() argument selector in
// reversed DOM tree traversal order, we can get the :has() matching result of
// the elements in the subtree. By caching these results, we can prevent
// unnecessary :has() matching operation. (Please refer the comments of
// HasArgumentSubtreeIterator in has_argument_match_context.h)
//
// Caching all matching results of the elements in the subtree is a very memory
// consuming approach. To prevent the large and inefficient cache memory
// consumption, HasSelectorMatchContext stores following flags for an
// element.
//
// - flag1 (Checked) : Indicates that the :has() match result of the element
//     was already checked.
//
// - flag2 (Matched) : Indicates that the element was already checked as
//     matched.
//
// - flag3 (AllDescendantsOrNextSiblingsChecked) : Indicates that all the
//     not-cached descendant elements (or all the not-cached next sibling
//     elements) of the element were already checked as not-matched.
//     When the :has() argument matching traversal is stopped, this flag is set
//     on the stopped element and the next sibling element of its ancestors to
//     mark already traversed subtree.
//
// - flag4 (SomeChildrenChecked) : Indicates that some children of the element
//     were already checked. This flag is set on the parent of the
//     kAllDescendantsOrNextSiblingsChecked element.
//     If the parent of an not-cached element has this flag set, we can
//     determine whether the element is 'already checked as not-matched' or
//     'not yet checked' by checking the AllDescendantsOrNextSiblingsChecked
//     flag of its previous sibling elements.
//
// Example)  subject.match(':has(.a)')
//  - DOM
//      <div id=subject>
//        <div id=d1>
//          <div id=d11></div>
//        </div>
//        <div id=d2>
//          <div id=d21></div>
//          <div id=d22 class=a>
//            <div id=d221></div>
//          </div>
//          <div id=d23></div>
//        </div>
//        <div id=d3>
//          <div id=d31></div>
//        </div>
//        <div id=d4></div>
//      </div>
//
//  - Cache
//      |    id    |  flag1  |  flag2  |  flag3  |  flag4  | actual state |
//      | -------- | ------- | ------- | ------- | ------- | ------------ |
//      |  subject |    1    |    1    |    0    |    1    |    matched   |
//      |    d1    |    -    |    -    |    -    |    -    |  not checked |
//      |    d11   |    -    |    -    |    -    |    -    |  not checked |
//      |    d2    |    1    |    1    |    0    |    1    |    matched   |
//      |    d21   |    -    |    -    |    -    |    -    |  not checked |
//      |    d22   |    1    |    0    |    1    |    0    |  not matched |
//      |    d221  |    -    |    -    |    -    |    -    |  not matched |
//      |    d23   |    -    |    -    |    -    |    -    |  not matched |
//      |    d3    |    1    |    0    |    1    |    0    |  not matched |
//      |    d31   |    -    |    -    |    -    |    -    |  not matched |
//      |    d4    |    -    |    -    |    -    |    -    |  not matched |
//
//  - How to check elements that are not in the cache.
//    - d1 :   1. Check parent(subject). Parent is 'SomeChildrenChecked'.
//             2. Traverse to previous siblings to find an element with the
//                flag3 (AllDescendantsOrNextSiblingsChecked).
//             >> not checked because no previous sibling with the flag set.
//    - d11 :  1. Check parent(d1). Parent is not cached.
//             2. Traverse to the parent(p1).
//             3. Check parent(subject). Parent is 'SomeChildrenChecked'.
//             4. Traverse to previous siblings to find an element with the
//                flag3 (AllDescendantsOrNextSiblingsChecked).
//             >> not checked because no previous sibling with the flag set.
//    - d21 :  1. Check parent(d2). Parent is 'SomeChildrenChecked'.
//             2. Traverse to previous siblings to find an element with the
//                flag3 (AllDescendantsOrNextSiblingsChecked).
//             >> not checked because no previous sibling with the flag set.
//    - d221 : 1. Check parent(d2).
//                Parent is 'AllDescendantsOrNextSiblingsChecked'.
//             >> not matched
//    - d23 :  1. Check parent(d2). Parent is 'SomeChildrenChecked'.
//             2. Traverse to previous siblings to find an element with the
//                flag3 (AllDescendantsOrNextSiblingsChecked).
//             >> not matched because d22 is
//                'AllDescendantsOrNextSiblingsChecked'.
//    - d31 :  1. Check parent(d3).
//                Parent is 'AllDescendantsOrNextSiblingsChecked'.
//             >> not matched
//    - d4 :   1. Check parent(subject). Parent is 'SomeChildrenChecked'.
//             2. Traverse to previous siblings to find an element with the
//                flag3 (AllDescendantsOrNextSiblingsChecked).
//             >> not matched because d3 is
//                'AllDescendantsOrNextSiblingsChecked'.
//
// Please refer the has_matched_cache_test.cc for other cases.
enum HasSelectorMatchResult : uint8_t {
  kNotCached = 0,
  kChecked = 1 << 0,
  kMatched = 1 << 1,
  kAllDescendantsOrNextSiblingsChecked = 1 << 2,
  kSomeChildrenChecked = 1 << 3,
};

using ElementHasMatchedMap = HeapHashMap<Member<const Element>, uint8_t>;
using HasMatchedCache = HeapHashMap<String, Member<ElementHasMatchedMap>>;

// HasMatchedCacheScope is the stack-allocated scoping class for :has
// matching cache.
//
// This class has a hashmap to hold the matching status, so the cache
// lifecycle follows the lifecycle of the HasMatchedCacheScope instance.
// (The hashmap for caching will be created at the construction of a
// HasMatchedCacheScope instance, and removed at the destruction of the
// instance)
//
// void SomeFunction() {
//   HasMatchedCacheScope cache_scope; // A cache will be created here.
//   // Can use the created cache here.
// } // The cache will be deleted here.
//
// The scope instance is allocated in the function-call stack, so the
// allocation can be nested. In this case, nested cache scope should not
// override the previous cache scope for a better cache hit ratio.
//
// void SomeFunction2() {
//   HasMatchedCacheScope cache_scope2;
//   // Use the cache in the cache_scope1.
//   // The cache in the cache_scope2 will not be used.
// }
//
// void SomeFunction1() {
//   HasMatchedCacheScope cache_scope1;
//   // Use the cache in the cache_scope1
//   SomeFunction2();
// }
//
// To make this simple, the first allocated instance on the call stack will
// be held in the Document instance. (The instance registers itself in the
// contructor and deregisters itself in the destructor) This is based on
// the restriction : The HasMatchedCacheScope is allowed to use only in the
// sequences on the blink main thread.
//
// The cache is valid until the DOM doesn't mutate, so any DOM mutations
// inside the cache scope is not allowed because it will make incorrect
// matching behavior.
//
// The cache keeps all the :has() matching status regardless of location.
// (e.g. '.a:has(.b) .c', '.a .b:has(.c)', ':is(.a:has(.b) .c) .d', ...)
//
// It stores the matching status of a :has simple selector. For example,
// when we have the selector '.a:has(.b) .c', during the selector matching
// sequence, matching result for ':has(.b)' will be inserted into the cache.
//
// To differentiate multiple :has() simple selectors, the argument selector
// text is selected as a cache key. For example, if we already have the result
// of ':has(.a)' in the cache with cache key '.a', and we have the selectors
// '.b:has(.a) .c' and '.b .c:has(a)' to be matched, then the matching
// overhead of those 2 selectors will be similar with the overhead of '.a .c'
// because we can get the result of ':has(.a)' from the cache with the cache
// key '.a'.
//
// The cache uses 2 dimensional hash map to store the matching status.
// - hashmap[<argument-selector>][<element>] = <match_result>
//
// ElementHasMatchedMap is a hash map that stores the :has(<argument-selector>)
// matching result for each element.
// - hashmap[<element>] = <match_result>
class CORE_EXPORT HasMatchedCacheScope {
  STACK_ALLOCATED();

 public:
  explicit HasMatchedCacheScope(Document*);
  ~HasMatchedCacheScope();

  // Context provides getter and setter of the cached :has() selector match
  // result in ElementHasMatchedMap.
  class CORE_EXPORT Context {
    STACK_ALLOCATED();

   public:
    Context() = delete;
    Context(const Document*, const HasArgumentMatchContext&);

    uint8_t SetMatchedAndGetOldResult(Element* element);

    void SetChecked(Element* element);

    void SetAllTraversedElementsAsChecked(Element* last_traversed_element,
                                          int last_traversed_depth);

    uint8_t GetResult(Element*) const;

    bool AlreadyChecked(Element*) const;

   private:
    friend class HasMatchedCacheScopeContextTest;

    uint8_t SetResultAndGetOld(Element*, uint8_t match_result);

    void SetTraversedElementAsChecked(Element* traversed_element,
                                      Element* parent);

    bool HasSiblingsWithAllDescendantsOrNextSiblingsChecked(Element*) const;
    bool HasAncestorsWithAllDescendantsOrNextSiblingsChecked(Element*) const;

    ElementHasMatchedMap& map_;
    const HasArgumentMatchContext& argument_match_context_;
  };

 private:
  static ElementHasMatchedMap& GetCacheForSelector(const Document*,
                                                   const CSSSelector*);

  HasMatchedCache has_matched_cache_;

  Document* document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_MATCHED_CACHE_SCOPE_H_
