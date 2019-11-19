// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_BIDI_PARAGRAPH_H_

#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <unicode/ubidi.h>

namespace blink {

class ComputedStyle;

// NGBidiParagraph resolves bidirectional runs in a paragraph using ICU BiDi.
// http://userguide.icu-project.org/transforms/bidi
//
// Given a string of a paragraph, it runs Unicode Bidirectional Algorithm in
// UAX#9 and create logical runs.
// http://unicode.org/reports/tr9/
// It can also create visual runs once lines breaks are determined.
class NGBidiParagraph {
  STACK_ALLOCATED();

 public:
  NGBidiParagraph() = default;
  ~NGBidiParagraph();

  // Splits the given paragraph to bidi runs and resolves the bidi embedding
  // level of each run.
  // Returns false on failure. Nothing other than the destructor should be
  // called.
  bool SetParagraph(const String&, const ComputedStyle&);

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
