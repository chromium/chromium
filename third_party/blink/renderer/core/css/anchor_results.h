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

// An AnchorItem represents an anchor query, i.e. either anchor(...)
// or anchor-size(). Its purpose is to act as the key for the hash map
// in AnchorResults, which can answer anchor queries based on predefined
// results.
class CORE_EXPORT AnchorItem : public GarbageCollected<AnchorItem> {
 public:
  // TODO(crbug.com/41483417): Remove CalculationExpressionAnchorQueryNode.
  static AnchorItem* Create(Length::AnchorScope::Mode,
                            const CalculationExpressionNode&);
  scoped_refptr<const CalculationExpressionNode> ToExpressionNode() const;

  AnchorItem(Length::AnchorScope::Mode mode,
             CSSAnchorQueryType query_type,
             const AnchorSpecifierValue* anchor_specifier,
             float percentage,
             absl::variant<CSSAnchorValue, CSSAnchorSizeValue> value)
      : mode_(mode),
        query_type_(query_type),
        anchor_specifier_(anchor_specifier),
        percentage_(percentage),
        value_(value) {
    CHECK(anchor_specifier);
  }

  bool operator==(const AnchorItem& other) const {
    return mode_ == other.mode_ && query_type_ == other.query_type_ &&
           percentage_ == other.percentage_ &&
           base::ValuesEquivalent(anchor_specifier_, other.anchor_specifier_) &&
           value_ == other.value_;
  }
  bool operator!=(const AnchorItem& other) const { return !operator==(other); }

  Length::AnchorScope::Mode GetMode() const { return mode_; }

  unsigned GetHash() const {
    unsigned hash = 0;
    WTF::AddIntToHash(hash, WTF::HashInt(mode_));
    WTF::AddIntToHash(hash, WTF::HashInt(query_type_));
    WTF::AddIntToHash(hash, anchor_specifier_->GetHash());
    WTF::AddIntToHash(hash, WTF::HashFloat(percentage_));
    if (query_type_ == CSSAnchorQueryType::kAnchor) {
      WTF::AddIntToHash(hash, WTF::HashInt(absl::get<CSSAnchorValue>(value_)));
    } else {
      CHECK_EQ(query_type_, CSSAnchorQueryType::kAnchorSize);
      WTF::AddIntToHash(hash,
                        WTF::HashInt(absl::get<CSSAnchorSizeValue>(value_)));
    }
    return hash;
  }

  void Trace(Visitor*) const;

 private:
  Length::AnchorScope::Mode mode_;
  CSSAnchorQueryType query_type_;
  Member<const AnchorSpecifierValue> anchor_specifier_;
  float percentage_;
  absl::variant<CSSAnchorValue, CSSAnchorSizeValue> value_;
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
static_assert(std::is_polymorphic_v<Length::AnchorEvaluator>);

// An implementation of Length::AnchorEvaluator which simply fetches
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
class CORE_EXPORT AnchorResults : public Length::AnchorEvaluator {
  DISALLOW_NEW();

 public:
  std::optional<LayoutUnit> Evaluate(const CalculationExpressionNode&) override;

  void Set(Length::AnchorScope::Mode,
           const CalculationExpressionNode&,
           std::optional<LayoutUnit>);
  void Clear();

  // Used for invalidation, see class comment.
  bool IsEmpty() const { return map_.empty(); }
  bool IsAnyResultDifferent(Length::AnchorEvaluator*) const;

  void Trace(Visitor*) const;

 private:
  AnchorResultMap map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_RESULTS_H_
