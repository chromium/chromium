// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"

#include <algorithm>
#include <memory>

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
      candidate_range_(NullRange()),
      script_run_iterator_(
          std::make_unique<ScriptRunIterator>(buffer, buffer_size)),
      orientation_iterator_(
          run_orientation == FontOrientation::kVerticalMixed
              ? std::make_unique<OrientationIterator>(buffer,
                                                      buffer_size,
                                                      run_orientation)
              : nullptr),
      symbols_iterator_(std::make_unique<SymbolsIterator>(buffer, buffer_size)),
      last_split_(0),
      script_run_iterator_position_(0),
      orientation_iterator_position_(
          run_orientation == FontOrientation::kVerticalMixed ? 0
                                                             : buffer_size_),
      symbols_iterator_position_(0),
      at_end_(!buffer_size_) {}

template <class Iterator, typename SegmentationCategory>
void RunSegmenter::ConsumeIteratorPastLastSplit(
    std::unique_ptr<Iterator>& iterator,
    unsigned* iterator_position,
    SegmentationCategory* segmentation_category) {
  if (*iterator_position <= last_split_ && *iterator_position < buffer_size_) {
    while (iterator->Consume(iterator_position, segmentation_category)) {
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
  ConsumeIteratorPastLastSplit(orientation_iterator_,
                               &orientation_iterator_position_,
                               &candidate_range_.render_orientation);
  ConsumeIteratorPastLastSplit(symbols_iterator_, &symbols_iterator_position_,
                               &candidate_range_.font_fallback_priority);

  unsigned positions[] = {script_run_iterator_position_,
                          orientation_iterator_position_,
                          symbols_iterator_position_};

  last_split_ = *std::min_element(std::begin(positions), std::end(positions));

  candidate_range_.start = candidate_range_.end;
  candidate_range_.end = last_split_;
  *next_range = candidate_range_;

  at_end_ = last_split_ == buffer_size_;
  return true;
}

}  // namespace blink
