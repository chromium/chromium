// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_MATCHED_CACHE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_MATCHED_CACHE_SCOPE_H_

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class Document;

using ElementHasMatchedMap = HeapHashMap<Member<const Element>, bool>;
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
// - hashmap[<argument-selector>][<element>] = <boolean>
//
// A cache item has 3 status as below.
// - Matched:
//     - Checked :has(<argument-selector>) on <element> and matched
//     - Cache item with 'true' value
// - Checked:
//     - Checked :has(<argument-selector>) on <element> but not matched
//     - Cache item with 'false' value
// - NotChecked:
//     - Not checked :has(<argument-selector>) on <element> (default)
//     - Cache item doesn't exist
//
// During the selector matching operation on an element, the cache items
// will be inserted for every :has argument selector matching operations
// on the subtree elements of the element.
//
// When we have a style rule 'div.b:has(.a1,.a2,.a3,...,.an) {...}', and
// m number of elements in the descendant subtree of div.b elements, and
// o number of ancestors of div.b elements, the maximum number of
// inserted cache items will be n*(m+o)
//
// TODO(blee@igalia.com) Need to think about some restrictions that can
// help to limit the cache size. For example, compounding with universal.
// When the ':has' is compounded with universal selector(e.g. ':has(...)'
// or '*:has(...)'), cache size can be large (m will be the number of
// elements in the document, and o will be zero). Need to restrict the
// case of compounding ':has' with universal selector?
class HasMatchedCacheScope {
  STACK_ALLOCATED();

 public:
  explicit HasMatchedCacheScope(Document*);
  ~HasMatchedCacheScope();

  static ElementHasMatchedMap& GetCacheForSelector(const Document*,
                                                   const CSSSelector*);

 private:
  HasMatchedCache has_matched_cache_;

  Document* document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_MATCHED_CACHE_SCOPE_H_
