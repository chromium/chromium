// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_RUN_SEGMENTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_RUN_SEGMENTER_H_

#include <unicode/uscript.h>
#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/orientation_iterator.h"
#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"
#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"
#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A tool for segmenting runs prior to shaping, combining ScriptIterator,
// OrientationIterator and SmallCapsIterator, depending on orientaton and
// font-variant of the text run.
class PLATFORM_EXPORT RunSegmenter {
  STACK_ALLOCATED();

 public:
  // Indices into the UTF-16 buffer that is passed in
  struct PLATFORM_EXPORT RunSegmenterRange {
    unsigned start;
    unsigned end;
    UScriptCode script;
    OrientationIterator::RenderOrientation render_orientation;
    FontFallbackPriority font_fallback_priority;
  };

  // Initialize a RunSegmenter.
  RunSegmenter(const UChar* buffer, unsigned buffer_size, FontOrientation);

  bool Consume(RunSegmenterRange*);

  static RunSegmenterRange NullRange() {
    return {0, 0, USCRIPT_INVALID_CODE, OrientationIterator::kOrientationKeep,
            FontFallbackPriority::kText};
  }

 private:
  template <class Iterator, typename SegmentationCategory>
  void ConsumeIteratorPastLastSplit(
      std::unique_ptr<Iterator>& iterator,
      unsigned* iterator_position,
      SegmentationCategory* segmentation_category);

  unsigned buffer_size_;
  RunSegmenterRange candidate_range_;
  std::unique_ptr<ScriptRunIterator> script_run_iterator_;
  std::unique_ptr<OrientationIterator> orientation_iterator_;
  std::unique_ptr<SymbolsIterator> symbols_iterator_;
  unsigned last_split_;
  unsigned script_run_iterator_position_;
  unsigned orientation_iterator_position_;
  unsigned symbols_iterator_position_;
  bool at_end_;

  DISALLOW_COPY_AND_ASSIGN(RunSegmenter);
};

}  // namespace blink

#endif
