// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/orientation_iterator.h"

#include <memory>

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

// UAX #29 rule GB9 keeps Extend and ZWJ in the preceding grapheme cluster, and
// rule GB11 keeps a pictograph that a ZWJ joins to a preceding pictograph:
// \p{Extended_Pictographic} Extend* ZWJ x \p{Extended_Pictographic}.
bool ExtendsGraphemeCluster(UChar32 character,
                            UChar32 cluster_base,
                            bool after_zwj) {
  if (Character::IsGraphemeExtended(character)) {
    return true;
  }
  if (!RuntimeEnabledFeatures::EmojiZWJVerticalOrientationEnabled()) {
    return false;
  }
  if (character == uchar::kZeroWidthJoiner) {
    return true;
  }
  return after_zwj && unicode::IsExtendedPictographic(character) &&
         unicode::IsExtendedPictographic(cluster_base);
}

}  // namespace

OrientationIterator::OrientationIterator(base::span<const UChar> buffer,
                                         FontOrientation run_orientation)
    : utf16_iterator_(buffer), at_end_(buffer.empty()) {
  // There's not much point in segmenting by IsUprightInMixedVertical if the
  // text orientation is not "mixed".
  DCHECK_EQ(run_orientation, FontOrientation::kVerticalMixed);
}

bool OrientationIterator::Consume(wtf_size_t* orientation_limit,
                                  RenderOrientation* render_orientation) {
  if (at_end_)
    return false;

  RenderOrientation current_render_orientation = kOrientationInvalid;
  UChar32 cluster_base = 0;
  bool after_zwj = false;
  UChar32 next_u_char32;
  while (utf16_iterator_.Consume(next_u_char32)) {
    if (current_render_orientation == kOrientationInvalid ||
        !ExtendsGraphemeCluster(next_u_char32, cluster_base, after_zwj)) {
      cluster_base = next_u_char32;
      RenderOrientation previous_render_orientation =
          current_render_orientation;
      current_render_orientation =
          Character::IsUprightInMixedVertical(next_u_char32)
              ? kOrientationKeep
              : kOrientationRotateSideways;
      if (previous_render_orientation != current_render_orientation &&
          previous_render_orientation != kOrientationInvalid) {
        *orientation_limit = utf16_iterator_.Offset();
        *render_orientation = previous_render_orientation;
        return true;
      }
    }
    after_zwj = next_u_char32 == uchar::kZeroWidthJoiner;
    utf16_iterator_.Advance();
  }
  *orientation_limit = utf16_iterator_.Size();
  *render_orientation = current_render_orientation;
  at_end_ = true;
  return true;
}

}  // namespace blink
