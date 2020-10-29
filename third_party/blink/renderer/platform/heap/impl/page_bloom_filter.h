// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_PAGE_BLOOM_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_PAGE_BLOOM_FILTER_H_

#include "third_party/blink/renderer/platform/heap/impl/heap_page.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"

namespace blink {

// Bloom filter for Oilpan pages. Use counting to support correct deletion. This
// is needed for stack scanning to quickly check if an arbitrary address doesn't
// point inside Oilpan pages. May return false positives but never false
// negatives.
class PageBloomFilter {
 public:
  void Add(Address address) {
    filter_.Add(Hash(RoundToBlinkPageStart(address)));
  }

  void Remove(Address address) {
    filter_.Remove(Hash(RoundToBlinkPageStart(address)));
  }

  bool MayContain(Address address) const {
    return filter_.MayContain(Hash(RoundToBlinkPageStart(address)));
  }

 private:
  static constexpr size_t kNumberOfEntriesLog2 = 12;
  static constexpr size_t kNumberOfEntries = 1 << kNumberOfEntriesLog2;

  static unsigned Hash(Address address) {
    size_t value = reinterpret_cast<size_t>(address) >> kBlinkPageSizeLog2;
    value ^= value >> kNumberOfEntriesLog2;
    value ^= value >> (kNumberOfEntriesLog2 * 2);
    value &= kNumberOfEntries - 1;
    return static_cast<unsigned>(value);
  }

  WTF::BloomFilter<kNumberOfEntriesLog2> filter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_PAGE_BLOOM_FILTER_H_
