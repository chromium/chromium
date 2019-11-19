// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_CACHE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This object implements a cache for storing successful hit tests to DOM nodes
// in the visible viewport. The cache is cleared on dom modifications,
// scrolling, CSS style modifications.
//
// Multiple hit tests can occur when processing events. Typically the DOM
// doesn't change when each event is processed so in order to decrease the time
// spent processing the events a hit cache is useful. For example a GestureTap
// event will generate a series of simulated mouse events (move, down, up,
// click) with the same co-ordinates and ideally we'd like to do the hit test
// once and use the result for the targetting of each event.
//
// Some of the related design, motivation can be found in:
// https://docs.google.com/document/d/1b0NYAD4S9BJIpHGa4JD2HLmW28f2rUh1jlqrgpU3zVU/
//

// A cache size of 2 is used because it is relatively cheap to store;
// and the ping-pong behaviour of some of the HitTestRequest flags during
// Mouse/Touch/Pointer events can generate increased cache misses with
// size of 1.
#define HIT_TEST_CACHE_SIZE (2)

struct HitTestCacheEntry {
  DISALLOW_NEW();

  void Trace(blink::Visitor*);
  HitTestLocation location;
  HitTestResult result;

  void CacheValues(const HitTestCacheEntry&);
};

class CORE_EXPORT HitTestCache final : public GarbageCollected<HitTestCache> {
 public:
  HitTestCache() : update_index_(0), dom_tree_version_(0) {}

  // Check the cache for a possible hit and update |result| if
  // hit encountered; returning true. Otherwise false.
  bool LookupCachedResult(const HitTestLocation&,
                          HitTestResult&,
                          uint64_t dom_tree_version);

  void Clear();

  // Adds a HitTestResult to the cache.
  void AddCachedResult(const HitTestLocation&,
                       const HitTestResult&,
                       uint64_t dom_tree_version);

  void Trace(blink::Visitor*);

 private:
  // The below UMA values reference a validity region. This code has not
  // been written yet; and exact matches are only supported but the
  // UMA enumerations have been added for future support.

  // These values are reported in UMA as the "EventHitTest" enumeration.
  // Do not reorder, append new values at the end, deprecate old
  // values and update histograms.xml.
  enum class HitHistogramMetric {
    MISS,                 // Miss, not found in cache.
    MISS_EXPLICIT_AVOID,  // Miss, callee asked to explicitly avoid cache.
    MISS_VALIDITY_RECT_MATCHES,  // Miss, validity region matches, type doesn't.
    HIT_EXACT_MATCH,             // Hit, exact point matches.
    HIT_REGION_MATCH,            // Hit, validity region matches.
    MAX_HIT_METRIC = HIT_REGION_MATCH,
  };

  unsigned update_index_;

  HeapVector<HitTestCacheEntry, HIT_TEST_CACHE_SIZE> items_;
  uint64_t dom_tree_version_;
  DISALLOW_COPY_AND_ASSIGN(HitTestCache);
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::HitTestCacheEntry)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_CACHE_H_
