// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"

namespace blink {

bool NGBidiParagraph::SetParagraph(const String& text,
                                   const ComputedStyle& block_style) {
  if (UNLIKELY(block_style.GetUnicodeBidi() == UnicodeBidi::kPlaintext))
    return SetParagraph(text, absl::nullopt);
  return SetParagraph(text, block_style.Direction());
}

}  // namespace blink
