/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/surrounding_text.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/backwards_character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

namespace blink {

namespace {

EphemeralRange ComputeRangeFromFrameSelection(LocalFrame* frame) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  return frame->Selection()
      .ComputeVisibleSelectionInDOMTree()
      .ToNormalizedEphemeralRange();
}

}  // namespace

SurroundingText::SurroundingText(LocalFrame* frame, wtf_size_t max_length)
    : SurroundingText(ComputeRangeFromFrameSelection(frame), max_length) {}

SurroundingText::SurroundingText(const EphemeralRange& range,
                                 wtf_size_t max_length)
    : start_offset_in_text_content_(0), end_offset_in_text_content_(0) {
  const Position start_position = range.StartPosition();
  const Position end_position = range.EndPosition();
  const wtf_size_t half_max_length = max_length / 2;

  Document* document = start_position.GetDocument();

  // The position will have no document if it is null (as in no position).
  if (!document || !document->documentElement())
    return;
  DCHECK(!document->NeedsLayoutTreeUpdate());

  // The forward range starts at the selection end and ends at the document's
  // or the input element's end. It will then be updated to only contain the
  // text in the right range around the selection.
  DCHECK_EQ(RootEditableElementOf(start_position),
            RootEditableElementOf(end_position));
  Element* const root_editable = RootEditableElementOf(start_position);
  Element* const root_element =
      root_editable ? root_editable : document->documentElement();

  // Do not create surrounding text if start or end position is within a
  // control.
  if (HTMLFormControlElement::EnclosingFormControlElement(
          start_position.ComputeContainerNode()) ||
      HTMLFormControlElement::EnclosingFormControlElement(
          end_position.ComputeContainerNode()))
    return;

  CharacterIterator forward_iterator(
      end_position,
      Position::LastPositionInNode(*root_element).ParentAnchoredEquivalent(),
      TextIteratorBehavior::Builder().SetStopsOnFormControls(true).Build());
  if (!forward_iterator.AtEnd())
    forward_iterator.Advance(max_length - half_max_length);

  // Same as with the forward range but with the backward range. The range
  // starts at the document's or input element's start and ends at the selection
  // start and will be updated.
  BackwardsCharacterIterator backwards_iterator(
      EphemeralRange(Position::FirstPositionInNode(*root_element)
                         .ParentAnchoredEquivalent(),
                     start_position),
      TextIteratorBehavior::Builder()
          .SetStopsOnFormControls(true)
          .SetEmitsPunctuationForReplacedElements(true)
          .Build());
  if (!backwards_iterator.AtEnd())
    backwards_iterator.Advance(half_max_length);

  // Upon some conditions backwards iterator yields invalid EndPosition() which
  // causes a crash in TextIterator::RangeLength().
  // TODO(editing-dev): Fix BackwardsCharacterIterator, http://crbug.com/758438.
  if (backwards_iterator.EndPosition() > start_position ||
      end_position > forward_iterator.StartPosition())
    return;

  const TextIteratorBehavior behavior =
      TextIteratorBehavior::NoTrailingSpaceRangeLengthBehavior();
  const Position content_start = backwards_iterator.EndPosition();
  const Position content_end = forward_iterator.StartPosition();
  start_offset_in_text_content_ =
      TextIterator::RangeLength(content_start, start_position, behavior);
  end_offset_in_text_content_ =
      TextIterator::RangeLength(content_start, end_position, behavior);
  text_content_ = PlainText(
      EphemeralRange(content_start, content_end),
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
}

String SurroundingText::TextContent() const {
  return text_content_;
}

wtf_size_t SurroundingText::StartOffsetInTextContent() const {
  return start_offset_in_text_content_;
}

wtf_size_t SurroundingText::EndOffsetInTextContent() const {
  return end_offset_in_text_content_;
}

bool SurroundingText::IsEmpty() const {
  return text_content_.empty();
}

}  // namespace blink
