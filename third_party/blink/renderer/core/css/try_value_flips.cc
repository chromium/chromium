// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/try_value_flips.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"

namespace blink {

namespace {

struct TrySides {
  CSSPropertyID block_start;
  CSSPropertyID block_end;
  CSSPropertyID inline_start;
  CSSPropertyID inline_end;
};

TrySides FlipBlock(const TrySides& other) {
  return TrySides{
      .block_start = other.block_end,
      .block_end = other.block_start,
      .inline_start = other.inline_start,
      .inline_end = other.inline_end,
  };
}

TrySides FlipInline(const TrySides& other) {
  return TrySides{
      .block_start = other.block_start,
      .block_end = other.block_end,
      .inline_start = other.inline_end,
      .inline_end = other.inline_start,
  };
}

TrySides FlipStart(const TrySides& other) {
  return TrySides{
      .block_start = other.inline_start,
      .block_end = other.inline_end,
      .inline_start = other.block_start,
      .inline_end = other.block_end,
  };
}

TrySides FlipTactic(const TrySides& other, TryTactic tactic) {
  switch (tactic) {
    case TryTactic::kNone:
      return other;
    case TryTactic::kFlipBlock:
      return FlipBlock(other);
    case TryTactic::kFlipInline:
      return FlipInline(other);
    case TryTactic::kFlipStart:
      return FlipStart(other);
  }
}

TrySides FlipSides(const TrySides& sides, const TryTacticList& tactic_list) {
  TrySides result = sides;
  for (TryTactic tactic : tactic_list) {
    result = FlipTactic(result, tactic);
  }
  return result;
}

struct TrySize {
  CSSPropertyID block_size;
  CSSPropertyID inline_size;
};

TrySize FlipSize(const TrySize& other, const TryTacticList& tactic_list) {
  // A size is only flipped if kFlipStart is present (which may only appear
  // once). kFlipBlock/kFlipInline has no effect.
  bool flip = std::any_of(
      tactic_list.begin(), tactic_list.end(),
      [](TryTactic tactic) { return tactic == TryTactic::kFlipStart; });
  return flip ? TrySize{.block_size = other.inline_size,
                        .inline_size = other.block_size}
              : other;
}

}  // namespace

const CSSPropertyValueSet* TryValueFlips::FlipSet(
    const TryTacticList& tactic_list) {
  if (tactic_list == kNoTryTactics) {
    return nullptr;
  }

  // Insets
  TrySides unflipped_insets{.block_start = CSSPropertyID::kInsetBlockStart,
                            .block_end = CSSPropertyID::kInsetBlockEnd,
                            .inline_start = CSSPropertyID::kInsetInlineStart,
                            .inline_end = CSSPropertyID::kInsetInlineEnd};
  TrySides flipped_insets = FlipSides(unflipped_insets, tactic_list);

  // Sizing
  TrySize unflipped_size{.block_size = CSSPropertyID::kBlockSize,
                         .inline_size = CSSPropertyID::kInlineSize};
  TrySize unflipped_min_size{.block_size = CSSPropertyID::kMinBlockSize,
                             .inline_size = CSSPropertyID::kMinInlineSize};
  TrySize unflipped_max_size{.block_size = CSSPropertyID::kMaxBlockSize,
                             .inline_size = CSSPropertyID::kMaxInlineSize};
  TrySize flipped_size = FlipSize(unflipped_size, tactic_list);
  TrySize flipped_min_size = FlipSize(unflipped_min_size, tactic_list);
  TrySize flipped_max_size = FlipSize(unflipped_max_size, tactic_list);

  constexpr wtf_size_t kMaxDeclarations = 10;
  HeapVector<CSSPropertyValue, kMaxDeclarations> declarations;

  auto add_if_flipped = [&declarations](CSSPropertyID from, CSSPropertyID to) {
    if (from != to) {
      declarations.push_back(CSSPropertyValue(
          CSSPropertyName(from),
          *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(to)));
    }
  };

  add_if_flipped(CSSPropertyID::kInsetBlockStart, flipped_insets.block_start);
  add_if_flipped(CSSPropertyID::kInsetBlockEnd, flipped_insets.block_end);
  add_if_flipped(CSSPropertyID::kInsetInlineStart, flipped_insets.inline_start);
  add_if_flipped(CSSPropertyID::kInsetInlineEnd, flipped_insets.inline_end);
  add_if_flipped(CSSPropertyID::kBlockSize, flipped_size.block_size);
  add_if_flipped(CSSPropertyID::kInlineSize, flipped_size.inline_size);
  add_if_flipped(CSSPropertyID::kMinBlockSize, flipped_min_size.block_size);
  add_if_flipped(CSSPropertyID::kMinInlineSize, flipped_min_size.inline_size);
  add_if_flipped(CSSPropertyID::kMaxBlockSize, flipped_max_size.block_size);
  add_if_flipped(CSSPropertyID::kMaxInlineSize, flipped_max_size.inline_size);

  return ImmutableCSSPropertyValueSet::Create(
      declarations.data(), declarations.size(), kHTMLStandardMode);
}

}  // namespace blink
