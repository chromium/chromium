// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/text_utils.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

float ComputeTextWidth(const StringView& text, const ComputedStyle& style) {
  if (text.empty()) {
    return 0;
  }
  // TODO(crbug.com/1229581): Re-implement this without TextRun.
  bool directional_override = style.RtlOrdering() == EOrder::kVisual;
  return style.GetFont().Width(
      TextRun(text, BidiParagraph::BaseDirectionForStringOrLtr(text),
              directional_override));
}

}  // namespace blink
