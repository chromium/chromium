// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_STABLE_SORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_STABLE_SORT_H_

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/collection_support/utils.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// Oilpan-safe stable sort for HeapVector and spans of HeapVector elements.
//
// `std::stable_sort` and `std::ranges::stable_sort` allocate an off-heap
// temporary buffer (`std::unique_ptr<T[]>`) via `::operator new` and move
// elements into it, hiding GC references from Oilpan's conservative stack
// scanning and write barriers. `StableSort` instead sorts an index permutation
// and permutes elements in place using a stack-allocated temporary.
template <typename T, size_t Extent, typename Compare = std::less<>>
void StableSort(base::span<T, Extent> range, Compare comp = {}) {
  const size_t n = range.size();
  if (n <= 1) {
    return;
  }

  std::vector<size_t> indices(n);
  std::iota(indices.begin(), indices.end(), size_t{0});
  std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
    if (std::invoke(comp, range[a], range[b])) {
      return true;
    }
    if (std::invoke(comp, range[b], range[a])) {
      return false;
    }
    return a < b;
  });

  for (size_t i = 0; i < n; ++i) {
    if (indices[i] == i) {
      continue;
    }
    T temp(std::move(range[i]));
    size_t current = i;
    while (true) {
      const size_t next = indices[current];
      indices[current] = current;
      if (next == i) {
        break;
      }
      range[current] = std::move(range[next]);
      current = next;
    }
    range[current] = std::move(temp);
  }
}

template <internal::HeapCollectionType CollectionType,
          typename T,
          wtf_size_t inlineCapacity,
          typename Compare = std::less<>>
void StableSort(BasicHeapVector<CollectionType, T, inlineCapacity>& vector,
                Compare comp = {}) {
  StableSort(base::span(vector), std::move(comp));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_STABLE_SORT_H_
