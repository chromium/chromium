// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_RESULTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_RESULTS_H_

#include <optional>
#include <type_traits>
#include "base/memory/values_equivalent.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/anchor_evaluator.h"
#include "third_party/blink/renderer/core/css/anchor_query.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class ComputedStyle;

// An AnchorItem represents an anchor query in a give Mode, i.e. either
// anchor(...) or anchor-size(). Its purpose is to act as the key for the hash
// map in AnchorResults, which can answer anchor queries based on predefined
// results.
class CORE_EXPORT AnchorItem : public GarbageCollected<AnchorItem> {
 public:
  AnchorItem(AnchorEvaluator::Mode mode, AnchorQuery query)
      : mode_(mode), query_(query) {}

  bool operator==(const AnchorItem& other) const {
    return mode_ == other.mode_ && query_ == other.query_;
  }
  bool operator!=(const AnchorItem& other) const { return !operator==(other); }

  AnchorEvaluator::Mode GetMode() const { return mode_; }
  const AnchorQuery& Query() const { return query_; }

  unsigned GetHash() const {
    unsigned hash = 0;
    WTF::AddIntToHash(hash, WTF::HashInt(mode_));
    WTF::AddIntToHash(hash, WTF::HashInt(query_.GetHash()));
    return hash;
  }

  void Trace(Visitor*) const;

 private:
  AnchorEvaluator::Mode mode_;
  AnchorQuery query_;
};

struct AnchorItemHashTraits : WTF::MemberHashTraits<const blink::AnchorItem> {
  using TraitType =
      typename MemberHashTraits<const blink::AnchorItem>::TraitType;
  static unsigned GetHash(const TraitType& name) { return name->GetHash(); }
  static bool Equal(const TraitType& a, const TraitType& b) {
    return base::ValuesEquivalent(a, b);
  }
  // Set this flag to 'false', otherwise Equal above will see gibberish values
  // that aren't safe to call ValuesEquivalent on.
  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

using AnchorResultMap = HeapHashMap<Member<const AnchorItem>,
                                    std::optional<LayoutUnit>,
                                    AnchorItemHashTraits>;

// [blink-gc] Left-most base class 'AnchorEvaluator' of derived class
// 'AnchorResults' must be polymorphic.
class GC_PLUGIN_IGNORE("Suppress broken is-polymorphic-check") AnchorResults;
static_assert(std::is_polymorphic_v<AnchorEvaluator>);

// An implementation of AnchorEvaluator which simply fetches
// the results from a predefined map.
//
// The results are populated during interleaved style recalc from
// out-of-flow layout (StyleEngine::UpdateStyleForOutOfFlow),
// and then used by subsequent non-interleaved style recalcs.
// The results then persist until the next call to UpdateStyleForOutOfFlow,
// which clears the results before populating again.
//
// AnchorResults also keeps track any calls made to Evaluate
// that were not present in the map. This is to make it possible
// for UpdateStyleForOutOfFlow to know in advance if any result
// changed, and skip recalc entirely if possible.
//
// See also ResultCachingAnchorEvaluator.
class CORE_EXPORT AnchorResults : public AnchorEvaluator {
  DISALLOW_NEW();

 public:
  std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const ScopedCSSName* position_anchor,
      const std::optional<InsetAreaOffsets>&) override;
  std::optional<InsetAreaOffsets> ComputeInsetAreaOffsetsForLayout(
      const ScopedCSSName* position_anchor,
      InsetArea inset_area) override;
  std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder&) override;

  void Set(AnchorEvaluator::Mode,
           const AnchorQuery&,
           std::optional<LayoutUnit>);
  void Clear();

  // Used for invalidation, see class comment.
  bool IsEmpty() const { return map_.empty(); }
  bool IsAnyResultDifferent(const ComputedStyle&, AnchorEvaluator*) const;

  void Trace(Visitor*) const override;

 private:
  AnchorResultMap map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_RESULTS_H_
