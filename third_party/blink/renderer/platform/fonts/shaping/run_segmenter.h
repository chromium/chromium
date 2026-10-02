// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_RUN_SEGMENTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_RUN_SEGMENTER_H_

#include <unicode/uscript.h>

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/orientation_iterator.h"
#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"
#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"
#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// A tool for segmenting runs prior to shaping, combining ScriptIterator,
// OrientationIterator and SmallCapsIterator, depending on orientaton and
// font-variant of the text run.
class PLATFORM_EXPORT RunSegmenter {
  STACK_ALLOCATED();

 public:
  // Indices into the UTF-16 buffer that is passed in
  struct PLATFORM_EXPORT RunSegmenterRange {
    wtf_size_t start = 0;
    wtf_size_t end = 0;
    UScriptCode script = USCRIPT_INVALID_CODE;
    OrientationIterator::RenderOrientation render_orientation =
        OrientationIterator::kOrientationKeep;
    FontFallbackPriority font_fallback_priority = FontFallbackPriority::kText;
  };

  // Initialize a RunSegmenter.
  RunSegmenter(base::span<const UChar> buffer, FontOrientation);
  RunSegmenter(const RunSegmenter&) = delete;
  RunSegmenter& operator=(const RunSegmenter&) = delete;

  bool Consume(RunSegmenterRange*);

 private:
  template <class Iterator, typename SegmentationCategory>
  void ConsumeIteratorPastLastSplit(
      Iterator& iterator,
      wtf_size_t* iterator_position,
      SegmentationCategory* segmentation_category);

  wtf_size_t buffer_size_;
  RunSegmenterRange candidate_range_;
  ScriptRunIterator script_run_iterator_;
  std::optional<OrientationIterator> orientation_iterator_;
  SymbolsIterator symbols_iterator_;
  wtf_size_t last_split_ = 0;
  wtf_size_t script_run_iterator_position_ = 0;
  wtf_size_t orientation_iterator_position_ = 0;
  wtf_size_t symbols_iterator_position_ = 0;
  bool at_end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_RUN_SEGMENTER_H_
