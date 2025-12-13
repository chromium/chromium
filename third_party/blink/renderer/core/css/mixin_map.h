// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_MAP_H_

#include "third_party/blink/renderer/core/css/resolver/media_query_result.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class StyleRuleMixin;

// The set of mixins available when building a given RuleSet.
// Typically contains all mixins from a given TreeScope
// (and possibly non-overridden ones from parent TreeScopes,
// after we've done appropriate merging), plus information
// needed for invalidation.
struct MixinMap {
  DISALLOW_NEW();

 public:
  HeapHashMap<AtomicString, Member<StyleRuleMixin>> mixins;

  // Media queries that affected the contents of “mixins”.
  // We currently store only one set of flags, not specifically
  // which mixin would be invalidated by which media query.
  MediaQueryResultFlags media_query_result_flags;
  HeapVector<MediaQuerySetResult> media_query_set_results;

  // See StyleSheetCollection::mixin_generation_. This is only
  // set for the final (merged) MixinMaps for an entire scope,
  // not when extracting mixins from a single stylesheet.
  uint64_t generation = 0;

  // Add everything from “other” to this map, overwriting
  // any mixins that may already exist. Does not touch “generation”.
  void Merge(const MixinMap& other);

  void Trace(Visitor* visitor) const {
    visitor->Trace(mixins);
    visitor->Trace(media_query_set_results);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MIXIN_MAP_H_
