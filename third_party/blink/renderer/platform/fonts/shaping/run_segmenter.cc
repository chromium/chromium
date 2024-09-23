// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"

#include <algorithm>
#include <memory>

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"
#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"
#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/text/character.h"

namespace blink {

RunSegmenter::RunSegmenter(const UChar* buffer,
                           unsigned buffer_size,
                           FontOrientation run_orientation)
    : buffer_size_(buffer_size),
      script_run_iterator_(buffer, buffer_size),
      symbols_iterator_(buffer, buffer_size),
      at_end_(!buffer_size) {
  if (run_orientation == FontOrientation::kVerticalMixed) [[unlikely]] {
    orientation_iterator_.emplace(buffer, buffer_size, run_orientation);
  }
}

template <class Iterator, typename SegmentationCategory>
void RunSegmenter::ConsumeIteratorPastLastSplit(
    Iterator& iterator,
    unsigned* iterator_position,
    SegmentationCategory* segmentation_category) {
  if (*iterator_position <= last_split_ && *iterator_position < buffer_size_) {
    while (iterator.Consume(iterator_position, segmentation_category)) {
      if (*iterator_position > last_split_)
        return;
    }
  }
}

// Consume the input until the next range. Returns false if no more ranges are
// available.
bool RunSegmenter::Consume(RunSegmenterRange* next_range) {
  if (at_end_)
    return false;

  ConsumeIteratorPastLastSplit(script_run_iterator_,
                               &script_run_iterator_position_,
                               &candidate_range_.script);
  ConsumeIteratorPastLastSplit(symbols_iterator_, &symbols_iterator_position_,
                               &candidate_range_.font_fallback_priority);

  if (orientation_iterator_) [[unlikely]] {
    ConsumeIteratorPastLastSplit(*orientation_iterator_,
                                 &orientation_iterator_position_,
                                 &candidate_range_.render_orientation);
    unsigned positions[] = {script_run_iterator_position_,
                            symbols_iterator_position_,
                            orientation_iterator_position_};
    last_split_ = *base::ranges::min_element(positions);
  } else {
    last_split_ =
        std::min(script_run_iterator_position_, symbols_iterator_position_);
  }

  candidate_range_.start = candidate_range_.end;
  candidate_range_.end = last_split_;
  *next_range = candidate_range_;

  at_end_ = last_split_ == buffer_size_;
  return true;
}

}  // namespace blink
