// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_

#include <unicode/ubidi.h>

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;

// NGBidiParagraph resolves bidirectional runs in a paragraph using ICU BiDi.
// http://userguide.icu-project.org/transforms/bidi
//
// Given a string of a paragraph, it runs Unicode Bidirectional Algorithm in
// UAX#9 and create logical runs.
// http://unicode.org/reports/tr9/
// It can also create visual runs once lines breaks are determined.
class CORE_EXPORT NGBidiParagraph {
  STACK_ALLOCATED();

 public:
  NGBidiParagraph() = default;
  ~NGBidiParagraph();

  // Splits the given paragraph to bidi runs and resolves the bidi embedding
  // level of each run.
  // Returns false on failure. Nothing other than the destructor should be
  // called.
  bool SetParagraph(const String&, const ComputedStyle&);
  bool SetParagraph(const String&,
                    absl::optional<TextDirection> base_direction);

  // @return the entire text is unidirectional.
  bool IsUnidirectional() const {
    return ubidi_getDirection(ubidi_) != UBIDI_MIXED;
  }

  // The base direction (a.k.a. paragraph direction) of this block.
  // This is determined by the 'direction' property of the block, or by the
  // heuristic rules defined in UAX#9 if 'unicode-bidi: plaintext'.
  TextDirection BaseDirection() const { return base_direction_; }

  // Compute the base direction for a given string using the heuristic
  // rules defined in UAX#9.
  // This is generally determined by the first strong character.
  // http://unicode.org/reports/tr9/#The_Paragraph_Level
  static TextDirection BaseDirectionForString(const StringView&);

  struct Run {
    Run(unsigned start, unsigned end, UBiDiLevel level)
        : start(start), end(end), level(level) {
      DCHECK_GT(end, start);
    }

    unsigned Length() const { return end - start; }
    TextDirection Direction() const { return DirectionFromLevel(level); }

    bool operator==(const Run& other) const {
      return start == other.start && end == other.end && level == other.level;
    }

    unsigned start;
    unsigned end;
    UBiDiLevel level;
  };
  using Runs = Vector<Run, 32>;

  // Get a list of |Run| in the logical order (before bidi reorder.)
  // |text| must be the same one as |SetParagraph|.
  // This is higher-level API for |GetLogicalRun|.
  void GetLogicalRuns(const String& text, Runs* runs) const;

  // Returns the end offset of a logical run that starts from the |start|
  // offset.
  unsigned GetLogicalRun(unsigned start, UBiDiLevel*) const;

  // Create a list of indices in the visual order.
  // A wrapper for ICU |ubidi_reorderVisual()|.
  static void IndicesInVisualOrder(
      const Vector<UBiDiLevel, 32>& levels,
      Vector<int32_t, 32>* indices_in_visual_order_out);

 private:
  UBiDi* ubidi_ = nullptr;
  TextDirection base_direction_ = TextDirection::kLtr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_
