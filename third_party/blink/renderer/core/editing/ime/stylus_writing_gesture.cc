// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/input/stylus_writing_gesture.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/ime/stylus_writing_gesture.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {

namespace {

class StylusWritingTwoPointGesture : public StylusWritingGesture {
 public:
  ~StylusWritingTwoPointGesture() override = default;

 protected:
  StylusWritingTwoPointGesture(const gfx::Point& start_point,
                               const gfx::Point& end_point,
                               const String& text_alternative);

  // Gets the text range in input between the start and end points of this
  // gesture. Returns null if the gesture is not over valid text input.
  absl::optional<PlainTextRange> GestureRange(LocalFrame*);

  // End point of the gesure.
  gfx::Point end_point_;
};

class StylusWritingGestureDelete : public StylusWritingTwoPointGesture {
 public:
  ~StylusWritingGestureDelete() override = default;

  StylusWritingGestureDelete(const gfx::Point& start_point,
                             const gfx::Point& end_point,
                             const String& text_alternative,
                             const mojom::StylusWritingGestureGranularity granularity);
  bool MaybeApplyGesture(LocalFrame*) override;

 private:
  const mojom::StylusWritingGestureGranularity granularity_;
};

class StylusWritingGestureRemoveSpaces : public StylusWritingTwoPointGesture {
 public:
  ~StylusWritingGestureRemoveSpaces() override = default;

  StylusWritingGestureRemoveSpaces(const gfx::Point& start_point,
                                   const gfx::Point& end_point,
                                   const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;
};

class StylusWritingGestureAddText : public StylusWritingGesture {
 public:
  ~StylusWritingGestureAddText() override = default;

  StylusWritingGestureAddText(const gfx::Point& start_point,
                              const String& text_to_insert,
                              const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;

 private:
  // Text to insert for the add text gesture. This also includes adding space
  // character.
  String text_to_insert_;
};

std::unique_ptr<StylusWritingGesture> CreateGesture(
    mojom::blink::StylusWritingGestureDataPtr gesture_data) {
  gfx::Point start_point = gesture_data->start_point;
  String text_alternative = gesture_data->text_alternative;

  switch (gesture_data->action) {
    case mojom::blink::StylusWritingGestureAction::DELETE_TEXT: {
      return std::make_unique<blink::StylusWritingGestureDelete>(
          start_point, gesture_data->end_point.value(), text_alternative,
          gesture_data->granularity);
    }
    case mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT: {
      return std::make_unique<blink::StylusWritingGestureAddText>(
          start_point, gesture_data->text_to_insert, text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::REMOVE_SPACES: {
      return std::make_unique<blink::StylusWritingGestureRemoveSpaces>(
          start_point, gesture_data->end_point.value(), text_alternative);
    }
    default: {
      NOTREACHED();
      return nullptr;
    }
  }
}

// Gets the text range for continuous spaces, or range for first spaces found in
// given gesture range.
absl::optional<PlainTextRange> GetTextRangeForSpaces(
    PlainTextRange& gesture_range,
    const String& gesture_text) {
  wtf_size_t space_start = kNotFound;
  wtf_size_t space_end = kNotFound;
  // Use this boolean to set the start/end offsets of space range.
  bool space_found = false;

  for (wtf_size_t index = 0; index < gesture_text.length(); index++) {
    if (IsHTMLSpace(gesture_text[index])) {
      if (!space_found) {
        space_found = true;
        space_start = index;
      }
      space_end = index + 1;
    } else if (space_found) {
      break;
    }
  }

  if (!space_found)
    return absl::nullopt;

  // Return range for space wrt input text range.
  return PlainTextRange(space_start + gesture_range.Start(),
                        space_end + gesture_range.Start());
}

}  // namespace

// static
void StylusWritingGesture::ApplyGesture(
    LocalFrame* local_frame,
    mojom::blink::StylusWritingGestureDataPtr gesture_data) {
  if (!local_frame->GetEditor().CanEdit())
    return;

  if (!local_frame->Selection().RootEditableElementOrDocumentElement())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. See http://crbug.com/590369 for more details.
  local_frame->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  InputMethodController& input_method_controller =
      local_frame->GetInputMethodController();
  // Finish composition if its active before doing gesture actions.
  if (input_method_controller.HasComposition()) {
    input_method_controller.FinishComposingText(
        InputMethodController::kKeepSelection);
  }

  // Create gesture corresponding to gesture data action.
  std::unique_ptr<StylusWritingGesture> gesture =
      CreateGesture(std::move(gesture_data));
  if (!gesture->MaybeApplyGesture(local_frame)) {
    // If the Stylus writing gesture could not be applied due the gesture
    // coordinates not being over a valid text position in the current focused
    // input, then insert the alternative text recognized.
    local_frame->GetEditor().InsertText(gesture->text_alternative_,
                                        /* triggering_event = */ nullptr);
  }
}

StylusWritingGesture::StylusWritingGesture(const gfx::Point& start_point,
                                           const String& text_alternative)
    : start_point_(start_point), text_alternative_(text_alternative) {}

wtf_size_t StylusWritingGesture::GetStartTextIndex(LocalFrame* local_frame) {
  auto* frame_view = local_frame->View();
  DCHECK(frame_view);
  return local_frame->Selection().CharacterIndexForPoint(
      frame_view->ViewportToFrame(start_point_));
}

StylusWritingTwoPointGesture::StylusWritingTwoPointGesture(
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    const String& text_alternative)
    : StylusWritingGesture(start_point, text_alternative),
      end_point_(end_point) {}

absl::optional<PlainTextRange> StylusWritingTwoPointGesture::GestureRange(
    LocalFrame* local_frame) {
  auto* frame_view = local_frame->View();
  DCHECK(frame_view);
  Element* const root_editable_element =
      local_frame->Selection().RootEditableElementOrDocumentElement();
  EphemeralRange ephemeral_range = local_frame->GetEditor().RangeBetweenPoints(
      frame_view->ViewportToFrame(start_point_),
      frame_view->ViewportToFrame(end_point_));
  if (ephemeral_range.IsCollapsed())
    return absl::nullopt;

  PlainTextRange gesture_range =
      PlainTextRange::Create(*root_editable_element, ephemeral_range);

  if (gesture_range.IsNull() || gesture_range.Start() >= gesture_range.End()) {
    // Gesture points do not have valid offsets in input.
    return absl::nullopt;
  }

  return gesture_range;
}

StylusWritingGestureDelete::StylusWritingGestureDelete(
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    const String& text_alternative,
    const mojom::StylusWritingGestureGranularity granularity)
    : StylusWritingTwoPointGesture(start_point, end_point, text_alternative),
      granularity_(granularity) {}

bool StylusWritingGestureDelete::MaybeApplyGesture(LocalFrame* frame) {
  absl::optional<PlainTextRange> gesture_range = GestureRange(frame);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  // Delete the text between offsets and set cursor.
  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  // TODO(https://crbug.com/1379360): Add word granularity implementation here.
  DCHECK_EQ(granularity_, mojom::StylusWritingGestureGranularity::CHARACTER);
  input_method_controller.ReplaceText("", gesture_range.value());
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(gesture_range->Start(), gesture_range->Start()));
  return true;
}

StylusWritingGestureRemoveSpaces::StylusWritingGestureRemoveSpaces(
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    const String& text_alternative)
    : StylusWritingTwoPointGesture(start_point, end_point, text_alternative) {}

bool StylusWritingGestureRemoveSpaces::MaybeApplyGesture(LocalFrame* frame) {
  absl::optional<PlainTextRange> gesture_range = GestureRange(frame);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  Element* const root_editable_element =
      frame->Selection().RootEditableElementOrDocumentElement();
  String gesture_text =
      PlainText(gesture_range->CreateRange(*root_editable_element));
  absl::optional<PlainTextRange> space_range =
      GetTextRangeForSpaces(gesture_range.value(), gesture_text);
  if (!space_range.has_value())
    return false;

  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  input_method_controller.ReplaceText("", space_range.value());
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(space_range->Start(), space_range->Start()));
  return true;
}

StylusWritingGestureAddText::StylusWritingGestureAddText(
    const gfx::Point& start_point,
    const String& text_to_insert,
    const String& text_alternative)
    : StylusWritingGesture(start_point, text_alternative),
      text_to_insert_(text_to_insert) {}

bool StylusWritingGestureAddText::MaybeApplyGesture(LocalFrame* frame) {
  wtf_size_t gesture_text_index = GetStartTextIndex(frame);
  // When the gesture point is outside the input text range, we get a kNotFound.
  // Return false here to insert the text alternative.
  if (gesture_text_index == kNotFound)
    return false;

  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(gesture_text_index, gesture_text_index));
  frame->GetEditor().InsertText(text_to_insert_,
                                /* triggering_event = */ nullptr);
  return true;
}

}  // namespace blink
