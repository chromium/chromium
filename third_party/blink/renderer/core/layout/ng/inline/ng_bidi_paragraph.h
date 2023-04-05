// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_

#include <unicode/ubidi.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

namespace blink {

class ComputedStyle;

// NGBidiParagraph resolves bidirectional runs in a paragraph using ICU BiDi.
// http://userguide.icu-project.org/transforms/bidi
//
// See `BidiParagraph` for more details.
class CORE_EXPORT NGBidiParagraph : public BidiParagraph {
  STACK_ALLOCATED();

 public:
  // Splits the given paragraph to bidi runs and resolves the bidi embedding
  // level of each run.
  // Returns false on failure. Nothing other than the destructor should be
  // called.
  bool SetParagraph(const String&, const ComputedStyle&);
  using BidiParagraph::SetParagraph;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_
