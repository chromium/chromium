// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"

namespace blink {

NGBidiParagraph::~NGBidiParagraph() {
  ubidi_close(ubidi_);
}

bool NGBidiParagraph::SetParagraph(const String& text,
                                   const ComputedStyle& block_style) {
  if (UNLIKELY(block_style.GetUnicodeBidi() == UnicodeBidi::kPlaintext))
    return SetParagraph(text, absl::nullopt);
  return SetParagraph(text, block_style.Direction());
}

bool NGBidiParagraph::SetParagraph(
    const String& text,
    absl::optional<TextDirection> base_direction) {
  DCHECK(!ubidi_);
  ubidi_ = ubidi_open();

  UBiDiLevel para_level;
  if (base_direction) {
    base_direction_ = *base_direction;
    para_level = IsLtr(base_direction_) ? UBIDI_LTR : UBIDI_RTL;
  } else {
    para_level = UBIDI_DEFAULT_LTR;
  }

  ICUError error;
  ubidi_setPara(ubidi_, text.Characters16(), text.length(), para_level, nullptr,
                &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    ubidi_close(ubidi_);
    ubidi_ = nullptr;
    return false;
  }

  if (!base_direction)
    base_direction_ = DirectionFromLevel(ubidi_getParaLevel(ubidi_));

  return true;
}

TextDirection NGBidiParagraph::BaseDirectionForString(const StringView& text) {
  DCHECK(!text.Is8Bit());
  if (text.Is8Bit())
    return TextDirection::kLtr;
  UBiDiDirection direction =
      ubidi_getBaseDirection(text.Characters16(), text.length());
  return direction == UBIDI_RTL ? TextDirection::kRtl : TextDirection::kLtr;
}

unsigned NGBidiParagraph::GetLogicalRun(unsigned start,
                                        UBiDiLevel* level) const {
  int32_t end;
  ubidi_getLogicalRun(ubidi_, start, &end, level);
  return end;
}

void NGBidiParagraph::GetLogicalRuns(const String& text, Runs* runs) const {
  DCHECK(runs->empty());
  for (unsigned start = 0; start < text.length();) {
    UBiDiLevel level;
    unsigned end = GetLogicalRun(start, &level);
    DCHECK_GT(end, start);
    runs->emplace_back(start, end, level);
    start = end;
  }
}

void NGBidiParagraph::IndicesInVisualOrder(
    const Vector<UBiDiLevel, 32>& levels,
    Vector<int32_t, 32>* indices_in_visual_order_out) {
  // Check the size before passing the raw pointers to ICU.
  CHECK_EQ(levels.size(), indices_in_visual_order_out->size());
  ubidi_reorderVisual(levels.data(), levels.size(),
                      indices_in_visual_order_out->data());
}

}  // namespace blink
