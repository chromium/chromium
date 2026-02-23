// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_PRIORITY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_PRIORITY_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// The origin and importance criteria are evaluated together [1], hence we
// encode both into a single integer which will do the right thing when compared
// to another such encoded integer. See CascadeOrigin for more information on
// how that works.
//
// [1] https://www.w3.org/TR/css-cascade-3/#cascade-origin
inline uint32_t EncodeOriginImportance(CascadeOrigin origin, bool important) {
  if (important) {
    return static_cast<uint32_t>(origin) ^ 0xF;
  } else {
    return static_cast<uint32_t>(origin);
  }
}

// Tree order bits are flipped for important declarations to reverse the
// priority [1].
//
// [1] https://drafts.csswg.org/css-shadow/#shadow-cascading
inline uint32_t EncodeTreeOrder(uint16_t tree_order, bool important) {
  if (important) {
    return tree_order ^ 0xFFFF;
  } else {
    return tree_order;
  }
}

// Layer order bits are flipped for important declarations to reverse the
// priority [1].
//
// [1] https://drafts.csswg.org/css-cascade-5/#cascade-layering
inline uint64_t EncodeLayerOrder(uint16_t layer_order, bool important) {
  if (important) {
    return layer_order ^ 0xFFFF;
  } else {
    return layer_order;
  }
}

// The CascadePriority class encapsulates a subset of the cascading criteria
// described by css-cascade [1], and provides a way to compare priorities
// quickly by encoding all the information in a single integer.
//
// It encompasses, from most significant to least significant:
// Origin/importance; tree order, which is a number representing the
// shadow-including tree order [2]; inline style, which is a boolean incidating
// whether the declaration is in the style attribute [3]; layer order, which is
// a number representing the cascade layer order in the origin and tree scope
// [4]; rule_index/declaration_index, which contain the indices required to
// lookup a declaration in the underlying structure (e.g. a MatchResult); and
// finally already_applied, which is a bit that StyleCascade uses to know what
// has been applied during this call to StyleCascade::Apply.
//
// Note that rule_index is more important for ordering than one'd think;
// since we sort matched rules (in ElementRuleCollector::SortMatchedRules())
// by the tuple {layer, specificity, -proximity, style sheet index, rule index},
// and rule_index points into the vector of matched rules (it is _not_ a rule
// index into the style sheet), sorting by rule_index will inherently sort by
// that 5-tuple (except for the layer, which is irrelevant since we have our own
// layer ordering ahead of rule_index).
//
// [1] https://drafts.csswg.org/css-cascade/#cascading
// [2] https://drafts.csswg.org/css-shadow/#shadow-cascading
// [3] https://drafts.csswg.org/css-cascade/#style-attr
// [4] https://drafts.csswg.org/css-cascade-5/#layer-ordering
class CORE_EXPORT CascadePriority {
 public:
  // The declaration is important if this bit is set on the encoded priority.
  static constexpr uint64_t kImportantBit = 19;             // of high_bits_
  static constexpr uint64_t kOriginImportanceOffset = 16;   // of high_bits_
  static constexpr uint64_t kIsTryTacticsStyleOffset = 51;  // of low_bits_
  static constexpr uint64_t kIsTryStyleOffset = 50;         // of low_bits_
  static constexpr uint64_t kIsInlineStyleOffset = 49;      // of low_bits_
  static constexpr uint64_t kLayerOrderOffset = 33;         // of low_bits_
  static constexpr uint64_t kDeclarationIndexOffset = 1;    // of low_bits_
  static constexpr uint64_t kRuleIndexOffset = 17;          // of low_bits_

  static constexpr uint64_t kOriginImportanceMask =
      0xF << kOriginImportanceOffset;                 // of high_bits_
  static constexpr uint64_t kTreeOrderMask = 0xFFFF;  // of high_bits_
  static constexpr uint64_t kLayerOrderMask =
      static_cast<uint64_t>(0xFFFF) << kLayerOrderOffset;  // of low_bits_
  static constexpr uint64_t kDeclarationIndexMask =
      static_cast<uint64_t>(0xFFFF) << kDeclarationIndexOffset;  // of low_bits_
  static constexpr uint64_t kRuleIndexMask =
      static_cast<uint64_t>(0xFFFF) << kRuleIndexOffset;  // of low_bits_
  static constexpr uint64_t kAlreadyAppliedMask = 0x1;    // of low_bits_

  CascadePriority() : low_bits_(0), high_bits_(0) {}
  explicit CascadePriority(CascadeOrigin origin)
      : CascadePriority(origin,
                        /* important */ false,
                        /* tree_order */ 0,
                        /* is_inline_style */ false,
                        /* is_try_style */ false,
                        /* is_try_tactics_style */ false,
                        /* layer_order */ 0,
                        /* rule_index */ 0,
                        /* declaration_index */ 0) {}
  CascadePriority(CascadeOrigin origin, bool important)
      : CascadePriority(origin,
                        important,
                        /* tree_order */ 0,
                        /* is_inline_style */ false,
                        /* is_try_style */ false,
                        /* is_try_tactics_style */ false,
                        /* layer_order */ 0,
                        /* rule_index */ 0,
                        /* declaration_index */ 0) {}
  CascadePriority(CascadeOrigin origin, bool important, uint16_t tree_order)
      : CascadePriority(origin,
                        important,
                        tree_order,
                        /* is_inline_style */ false,
                        /* is_try_style */ false,
                        /* is_try_tactics_style */ false,
                        /* layer_order */ 0,
                        /* rule_index */ 0,
                        /* declaration_index */ 0) {}

  // For an explanation of 'tree_order', see css-shadow:
  // https://drafts.csswg.org/css-shadow/#shadow-cascading
  CascadePriority(CascadeOrigin origin,
                  bool important,
                  uint16_t tree_order,
                  bool is_inline_style,
                  bool is_try_style,
                  bool is_try_tactics_style,
                  uint16_t layer_order,
                  uint16_t rule_index,
                  uint16_t declaration_index)
      : CascadePriority(
            static_cast<uint64_t>(rule_index) << kRuleIndexOffset |
                static_cast<uint64_t>(declaration_index)
                    << kDeclarationIndexOffset |
                EncodeLayerOrder(layer_order, important) << kLayerOrderOffset |
                static_cast<uint64_t>(is_inline_style) << kIsInlineStyleOffset |
                static_cast<uint64_t>(is_try_style) << kIsTryStyleOffset |
                static_cast<uint64_t>(is_try_tactics_style)
                    << kIsTryTacticsStyleOffset,
            EncodeTreeOrder(tree_order, important) |
                EncodeOriginImportance(origin, important)
                    << kOriginImportanceOffset) {}
  CascadePriority(CascadePriority o, bool already_applied)
      : CascadePriority((o.low_bits_ & ~kAlreadyAppliedMask) | already_applied,
                        o.high_bits_) {}

  bool IsImportant() const { return (high_bits_ >> kImportantBit) & 1; }
  CascadeOrigin GetOrigin() const {
    uint64_t important_xor =
        (((~high_bits_ >> kImportantBit) & 1) - 1) & kOriginImportanceMask;
    return static_cast<CascadeOrigin>((high_bits_ ^ important_xor) >>
                                      kOriginImportanceOffset);
  }
  bool HasOrigin() const { return GetOrigin() != CascadeOrigin::kNone; }
  wtf_size_t GetDeclarationIndex() const {
    return (low_bits_ & kDeclarationIndexMask) >> kDeclarationIndexOffset;
  }
  wtf_size_t GetRuleIndex() const {
    return (low_bits_ & kRuleIndexMask) >> kRuleIndexOffset;
  }
  bool IsAlreadyApplied() const { return low_bits_ & kAlreadyAppliedMask; }
  bool IsInlineStyle() const { return (low_bits_ >> kIsInlineStyleOffset) & 1; }

  // https://drafts.csswg.org/css-anchor-position-1/#fallback-rule
  bool IsTryStyle() const { return (low_bits_ >> kIsTryStyleOffset) & 1; }

  // Returns a value that compares like CascadePriority, except that it
  // ignores the importance and all sorting criteria below layer order,
  // which allows us to compare if two CascadePriorities belong
  // to the same cascade layer.
  uint64_t ForLayerComparison() const {
    // Our value to compare is essentially 96 bits. Get the uppermost 64 bits
    // (we don't care about already_applied and position).
    uint64_t bits =
        (low_bits_ >> 32) | (static_cast<uint64_t>(high_bits_) << 32);

    // NOTE: This branch will get converted into a conditional move by the
    // compiler.
    if (bits & (1ull << (kImportantBit + 32))) {
      // Remove importance, which means; we need to clear the importance bit.
      // But if set, it has previously flipped some other interesting bits
      // (origin/importance, tree order and layer order), so we need to flip
      // them back before returning. (We do not flip the kTransition bit
      // in the encoded origin, nor the comes-from-inline-style bit.)
      bits ^= kOriginImportanceMask << 32;
      bits ^= kTreeOrderMask << 32;
      bits ^= kLayerOrderMask >> 32;
    }

    bits >>= kLayerOrderOffset - 32;  // Remove everything below layer_order.
    return bits;
  }

  bool operator>=(const CascadePriority& o) const {
    return high_bits_ > o.high_bits_ ||
           (high_bits_ == o.high_bits_ && low_bits_ >= o.low_bits_);
  }
  bool operator<(const CascadePriority& o) const {
    return high_bits_ < o.high_bits_ ||
           (high_bits_ == o.high_bits_ && low_bits_ < o.low_bits_);
  }
  bool operator==(const CascadePriority& o) const {
    return high_bits_ == o.high_bits_ && low_bits_ == o.low_bits_;
  }

 private:
  friend class StyleCascade;
  friend class StyleCascadeTest;

  CascadePriority(uint64_t low_bits, uint32_t high_bits)
      : low_bits_(low_bits), high_bits_(high_bits) {}

  //  Bit     0: already_applied
  //  Bit  1-16: declaration_index
  //  Bit 17-32: rule_index
  //  Bit 33-48: layer_order (encoded)
  //  Bit    49: is_inline_style
  //  Bit    50: is_try_style
  //  Bit    51: is_try_tactics_style
  //  (12 free bits)
  uint64_t low_bits_;

  //  Bit  0-15: tree_order (encoded)
  //  Bit 16-20: origin/importance (encoded; bit 19 is importance)
  //  (11 free bits)
  uint32_t high_bits_;

  // NOTE: Interpolations use the declaration_index field for CSSPropertyID,
  // and rule_index for entry index (need only 8 bits).
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_PRIORITY_H_
