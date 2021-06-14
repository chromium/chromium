// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_ITERATOR_TEXT_NODE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_ITERATOR_TEXT_NODE_HANDLER_H_

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator_behavior.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class InlineTextBox;
class LayoutText;
class LayoutTextFragment;
class TextIteratorTextState;

// TextIteratorTextNodeHandler extracts plain text from a text node by calling
// HandleTextNode() function. It should be used only by TextIterator.
class TextIteratorTextNodeHandler {
  STACK_ALLOCATED();

 public:
  TextIteratorTextNodeHandler(const TextIteratorBehavior&,
                              TextIteratorTextState*);
  TextIteratorTextNodeHandler(const TextIteratorTextNodeHandler&) = delete;
  TextIteratorTextNodeHandler& operator=(const TextIteratorTextNodeHandler&) =
      delete;

  const Text* GetNode() const { return text_node_; }

  // Returns true if more text is emitted without traversing to the next node.
  bool HandleRemainingTextRuns();

  // Returns true if a leading white space is emitted before a replaced element.
  bool FixLeadingWhiteSpaceForReplacedElement();

  void ResetCollapsedWhiteSpaceFixup();

  // Emit plain text from the given text node.
  void HandleTextNodeWhole(const Text*);

  // Variants that emit plain text within the given DOM offset range.
  void HandleTextNodeStartFrom(const Text*, unsigned start_offset);
  void HandleTextNodeEndAt(const Text*, unsigned end_offset);
  void HandleTextNodeInRange(const Text*,
                             unsigned start_offset,
                             unsigned end_offset);

 private:
  void HandlePreFormattedTextNode();
  void HandleTextBox();
  void HandleTextNodeFirstLetter(LayoutTextFragment*);
  bool ShouldHandleFirstLetter(const LayoutText&) const;
  bool ShouldProceedToRemainingText() const;
  void ProceedToRemainingText();
  wtf_size_t RestoreCollapsedTrailingSpace(InlineTextBox* next_text_box,
                                           wtf_size_t subrun_end);

  void HandleTextNodeWithLayoutNG();

  // Used when the visibility of the style should not affect text gathering.
  bool IgnoresStyleVisibility() const {
    return behavior_.IgnoresStyleVisibility();
  }

  bool ShouldFixLeadingWhiteSpaceForReplacedElement() const;

  // Emits |code_unit| before |offset| of characters in |text_node_|.
  void EmitChar16Before(UChar code_unit, unsigned offset);
  // Emits |code_unit| as replacement of a code unit after |offset| in
  // |text_node_|.
  void EmitReplacmentCodeUnit(UChar code_unit, unsigned offset);

  void EmitText(const LayoutText* layout_object,
                unsigned text_start_offset,
                unsigned text_end_offset);

  // The current text node and offset range, from which text should be emitted.
  const Text* text_node_ = nullptr;
  unsigned offset_ = 0;
  unsigned end_offset_ = 0;

  // Indicates if the text node is laid out with LayoutNG.
  bool uses_layout_ng_ = false;
  // UnitVector for text_node_. This is available only if uses_layout_ng_.
  NGOffsetMapping::UnitVector mapping_units_;
  wtf_size_t mapping_units_index_;

  InlineTextBox* text_box_ = nullptr;

  // Remember if we are in the middle of handling a pre-formatted text node.
  bool needs_handle_pre_formatted_text_node_ = false;
  // Used when deciding text fragment created by :first-letter should be looked
  // into.
  bool handled_first_letter_ = false;
  // Used when iteration over :first-letter text to save pointer to
  // remaining text box.
  InlineTextBox* remaining_text_box_ = nullptr;
  // Used to point to LayoutText object for :first-letter.
  LayoutText* first_letter_text_ = nullptr;

  // Used to do the whitespace collapsing logic.
  bool last_text_node_ended_with_collapsed_space_ = false;

  // Used when text boxes are out of order (Hebrew/Arabic w/ embedded LTR text)
  Vector<InlineTextBox*> sorted_text_boxes_;
  wtf_size_t sorted_text_boxes_position_ = 0;

  const TextIteratorBehavior behavior_;

  // Contains state of emitted text.
  TextIteratorTextState& text_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_ITERATOR_TEXT_NODE_HANDLER_H_
