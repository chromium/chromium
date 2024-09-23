// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/orientation_iterator.h"

#include <memory>

namespace blink {

OrientationIterator::OrientationIterator(const UChar* buffer,
                                         unsigned buffer_size,
                                         FontOrientation run_orientation)
    : utf16_iterator_(buffer, buffer_size), at_end_(!buffer_size) {
  // There's not much point in segmenting by IsUprightInMixedVertical if the
  // text orientation is not "mixed".
  DCHECK_EQ(run_orientation, FontOrientation::kVerticalMixed);
}

bool OrientationIterator::Consume(unsigned* orientation_limit,
                                  RenderOrientation* render_orientation) {
  if (at_end_)
    return false;

  RenderOrientation current_render_orientation = kOrientationInvalid;
  UChar32 next_u_char32;
  while (utf16_iterator_.Consume(next_u_char32)) {
    if (current_render_orientation == kOrientationInvalid ||
        !Character::IsGraphemeExtended(next_u_char32)) {
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
    utf16_iterator_.Advance();
  }
  *orientation_limit = utf16_iterator_.Size();
  *render_orientation = current_render_orientation;
  at_end_ = true;
  return true;
}

}  // namespace blink
