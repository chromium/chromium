// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/stylus_writing_gesture.h"

#include "third_party/blink/public/mojom/input/stylus_writing_gesture.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {

namespace {

class StylusWritingTwoRectGesture : public StylusWritingGesture {
 public:
  ~StylusWritingTwoRectGesture() override = default;

 protected:
  StylusWritingTwoRectGesture(const gfx::Rect& start_rect,
                              const gfx::Rect& end_rect,
                              const String& text_alternative);

  // Gets the text range in input between the start and end points of this
  // gesture. Returns null if the gesture is not over valid text input. Takes
  // granularity as a default parameter as not all gestures have a granularity.
  std::optional<PlainTextRange> GestureRange(
      LocalFrame*,
      const mojom::blink::StylusWritingGestureGranularity granularity =
          mojom::blink::StylusWritingGestureGranularity::CHARACTER);

  virtual std::optional<PlainTextRange> AdjustRange(
      std::optional<PlainTextRange> range,
      InputMethodController& input_method_controller) {
    return range;
  }

  // End rectangle of the gesture.
  gfx::Rect end_rect_;
};

class StylusWritingGestureDelete : public StylusWritingTwoRectGesture {
 public:
  ~StylusWritingGestureDelete() override = default;

  StylusWritingGestureDelete(
      const gfx::Rect& start_rect,
      const gfx::Rect& end_rect,
      const String& text_alternative,
      const mojom::blink::StylusWritingGestureGranularity granularity);
  bool MaybeApplyGesture(LocalFrame*) override;

 protected:
  std::optional<PlainTextRange> AdjustRange(std::optional<PlainTextRange>,
                                            InputMethodController&) override;

 private:
  const mojom::blink::StylusWritingGestureGranularity granularity_;
};

class StylusWritingGestureRemoveSpaces : public StylusWritingTwoRectGesture {
 public:
  ~StylusWritingGestureRemoveSpaces() override = default;

  StylusWritingGestureRemoveSpaces(const gfx::Rect& start_rect,
                                   const gfx::Rect& end_rect,
                                   const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;
};

class StylusWritingGestureSelect : public StylusWritingTwoRectGesture {
 public:
  ~StylusWritingGestureSelect() override = default;

  StylusWritingGestureSelect(
      const gfx::Rect& start_rect,
      const gfx::Rect& end_rect,
      const String& text_alternative,
      const mojom::StylusWritingGestureGranularity granularity);
  bool MaybeApplyGesture(LocalFrame*) override;

 protected:
  std::optional<PlainTextRange> AdjustRange(std::optional<PlainTextRange>,
                                            InputMethodController&) override;

 private:
  const mojom::StylusWritingGestureGranularity granularity_;
};

class StylusWritingGestureAddText : public StylusWritingGesture {
 public:
  ~StylusWritingGestureAddText() override = default;

  StylusWritingGestureAddText(const gfx::Rect& start_rect,
                              const String& text_to_insert,
                              const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;

 private:
  // Text to insert for the add text gesture. This also includes adding space
  // character.
  String text_to_insert_;
};

class StylusWritingGestureSplitOrMerge : public StylusWritingGesture {
 public:
  ~StylusWritingGestureSplitOrMerge() override = default;

  StylusWritingGestureSplitOrMerge(const gfx::Rect& start_rect,
                                   const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;
};

std::unique_ptr<StylusWritingGesture> CreateGesture(
    mojom::blink::StylusWritingGestureDataPtr gesture_data) {
  if (!gesture_data) {
    return nullptr;
  }
  String text_alternative = gesture_data->text_alternative;

  switch (gesture_data->action) {
    case mojom::blink::StylusWritingGestureAction::DELETE_TEXT: {
      if (!gesture_data->end_rect.has_value()) {
        return nullptr;
      }
      return std::make_unique<blink::StylusWritingGestureDelete>(
          gesture_data->start_rect, gesture_data->end_rect.value(),
          text_alternative, gesture_data->granularity);
    }
    case mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT: {
      return std::make_unique<blink::StylusWritingGestureAddText>(
          gesture_data->start_rect, gesture_data->text_to_insert,
          text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::REMOVE_SPACES: {
      if (!gesture_data->end_rect.has_value()) {
        return nullptr;
      }
      return std::make_unique<blink::StylusWritingGestureRemoveSpaces>(
          gesture_data->start_rect, gesture_data->end_rect.value(),
          text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE: {
      return std::make_unique<blink::StylusWritingGestureSplitOrMerge>(
          gesture_data->start_rect, text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::SELECT_TEXT: {
      if (!gesture_data->end_rect.has_value()) {
        return nullptr;
      }
      return std::make_unique<blink::StylusWritingGestureSelect>(
          gesture_data->start_rect, gesture_data->end_rect.value(),
          text_alternative, gesture_data->granularity);
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
  }
}

PlainTextRange ExpandWithWordGranularity(
    EphemeralRange ephemeral_range,
    Element* const root_editable_element,
    InputMethodController& input_method_controller) {
  SelectionInDOMTree expanded_selection = ExpandWithGranularity(
      SelectionInDOMTree::Builder().SetBaseAndExtent(ephemeral_range).Build(),
      TextGranularity::kWord, WordInclusion::kMiddle);
  PlainTextRange expanded_range = PlainTextRange::Create(
      *root_editable_element, expanded_selection.ComputeRange());
  return expanded_range;
}

std::optional<PlainTextRange> GestureRangeForPoints(
    LocalFrame* local_frame,
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    const mojom::blink::StylusWritingGestureGranularity granularity) {
  auto* frame_view = local_frame->View();
  DCHECK(frame_view);
  Element* const root_editable_element =
      local_frame->Selection().RootEditableElementOrDocumentElement();
  if (!root_editable_element) {
    return std::nullopt;
  }
  EphemeralRange ephemeral_range = local_frame->GetEditor().RangeBetweenPoints(
      frame_view->ViewportToFrame(start_point),
      frame_view->ViewportToFrame(end_point));
  if (ephemeral_range.IsCollapsed()) {
    return std::nullopt;
  }

  PlainTextRange gesture_range =
      PlainTextRange::Create(*root_editable_element, ephemeral_range);

  if (gesture_range.IsNull() || gesture_range.Start() >= gesture_range.End()) {
    // Gesture points do not have valid offsets in input.
    return std::nullopt;
  }
  switch (granularity) {
    case mojom::blink::StylusWritingGestureGranularity::CHARACTER:
      return gesture_range;
    case mojom::blink::StylusWritingGestureGranularity::WORD:
      return ExpandWithWordGranularity(ephemeral_range, root_editable_element,
                                       local_frame->GetInputMethodController());
    default:
      return std::nullopt;
  }
}

// Gets the text range for continuous spaces, or range for first spaces found in
// given gesture range.
std::optional<PlainTextRange> GetTextRangeForSpaces(
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
    return std::nullopt;

  // Return range for space wrt input text range.
  return PlainTextRange(space_start + gesture_range.Start(),
                        space_end + gesture_range.Start());
}

}  // namespace

// static
mojom::blink::HandwritingGestureResult StylusWritingGesture::ApplyGesture(
    LocalFrame* local_frame,
    mojom::blink::StylusWritingGestureDataPtr gesture_data) {
  if (!local_frame->GetEditor().CanEdit())
    return mojom::blink::HandwritingGestureResult::kFailed;

  if (!local_frame->Selection().RootEditableElementOrDocumentElement())
    return mojom::blink::HandwritingGestureResult::kFailed;

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
  if (gesture == nullptr) {
    return mojom::blink::HandwritingGestureResult::kUnsupported;
  }
  if (!gesture->MaybeApplyGesture(local_frame)) {
    // If the Stylus writing gesture could not be applied due the gesture
    // coordinates not being over a valid text position in the current focused
    // input, then insert the alternative text recognized.
    local_frame->GetEditor().InsertText(gesture->text_alternative_,
                                        /* triggering_event = */ nullptr);
    return mojom::blink::HandwritingGestureResult::kFallback;
  }
  return mojom::blink::HandwritingGestureResult::kSuccess;
}

StylusWritingGesture::StylusWritingGesture(const gfx::Rect& start_rect,
                                           const String& text_alternative)
    : start_rect_(start_rect), text_alternative_(text_alternative) {}

wtf_size_t StylusWritingGesture::GetStartTextIndex(LocalFrame* local_frame) {
  auto* frame_view = local_frame->View();
  DCHECK(frame_view);
  // This method should only be called on zero sized rectangles.
  DCHECK(start_rect_.IsEmpty());
  return local_frame->Selection().CharacterIndexForPoint(
      frame_view->ViewportToFrame(start_rect_.origin()));
}

StylusWritingTwoRectGesture::StylusWritingTwoRectGesture(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect,
    const String& text_alternative)
    : StylusWritingGesture(start_rect, text_alternative), end_rect_(end_rect) {}

std::optional<PlainTextRange> StylusWritingTwoRectGesture::GestureRange(
    LocalFrame* local_frame,
    const mojom::blink::StylusWritingGestureGranularity granularity) {
  Element* const root_editable_element =
      local_frame->Selection().RootEditableElementOrDocumentElement();
  if (!root_editable_element) {
    return std::nullopt;
  }
  if (start_rect_.IsEmpty() && end_rect_.IsEmpty()) {
    start_rect_.UnionEvenIfEmpty(end_rect_);
    start_rect_.InclusiveIntersect(root_editable_element->BoundsInWidget());
    return AdjustRange(
        GestureRangeForPoints(local_frame, start_rect_.left_center(),
                              start_rect_.right_center(), granularity),
        local_frame->GetInputMethodController());
  }
  start_rect_.InclusiveIntersect(root_editable_element->BoundsInWidget());
  std::optional<PlainTextRange> first_range =
      GestureRangeForPoints(local_frame, start_rect_.left_center(),
                            start_rect_.right_center(), granularity);
  end_rect_.InclusiveIntersect(root_editable_element->BoundsInWidget());
  std::optional<PlainTextRange> last_range =
      GestureRangeForPoints(local_frame, end_rect_.left_center(),
                            end_rect_.right_center(), granularity);
  if (!first_range.has_value() || !last_range.has_value()) {
    return std::nullopt;
  }
  // TODO(crbug.com/1411758): Add support for gestures with vertical text.

  // Combine the ranges' indices such that regardless of if the text is LTR or
  // RTL, the correct range is used.
  return AdjustRange(PlainTextRange(first_range->Start(), last_range->End()),
                     local_frame->GetInputMethodController());
}

StylusWritingGestureDelete::StylusWritingGestureDelete(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect,
    const String& text_alternative,
    const mojom::blink::StylusWritingGestureGranularity granularity)
    : StylusWritingTwoRectGesture(start_rect, end_rect, text_alternative),
      granularity_(granularity) {}

bool StylusWritingGestureDelete::MaybeApplyGesture(LocalFrame* frame) {
  std::optional<PlainTextRange> gesture_range =
      GestureRange(frame, granularity_);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  // Delete the text between offsets and set cursor.
  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(gesture_range->End(), gesture_range->End()));
  input_method_controller.DeleteSurroundingText(
      gesture_range->End() - gesture_range->Start(), 0);
  return true;
}

std::optional<PlainTextRange> StylusWritingGestureDelete::AdjustRange(
    std::optional<PlainTextRange> range,
    InputMethodController& input_method_controller) {
  if (!range.has_value() || range->length() < 2) {
    return range;
  }
  String input_text = input_method_controller.TextInputInfo().value;
  // When there is a space at the start and end of the gesture, remove one.
  if (IsHTMLSpaceNotLineBreak(input_text[range->Start()]) &&
      IsHTMLSpaceNotLineBreak(input_text[range->End() - 1])) {
    return PlainTextRange(range->Start() + 1, range->End());
  }
  // When there are spaces either side of the gesture, include one.
  if (input_text.length() > range->End() && range->Start() - 1 >= 0 &&
      IsHTMLSpaceNotLineBreak(input_text[range->Start() - 1]) &&
      !IsHTMLSpaceNotLineBreak(input_text[range->End() - 1])) {
    return PlainTextRange(range->Start() - 1, range->End());
  }
  return range;
}

StylusWritingGestureRemoveSpaces::StylusWritingGestureRemoveSpaces(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect,
    const String& text_alternative)
    : StylusWritingTwoRectGesture(start_rect, end_rect, text_alternative) {}

bool StylusWritingGestureRemoveSpaces::MaybeApplyGesture(LocalFrame* frame) {
  std::optional<PlainTextRange> gesture_range = GestureRange(frame);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  Element* const root_editable_element =
      frame->Selection().RootEditableElementOrDocumentElement();
  if (!root_editable_element) {
    return false;
  }
  String gesture_text =
      PlainText(gesture_range->CreateRange(*root_editable_element));
  std::optional<PlainTextRange> space_range =
      GetTextRangeForSpaces(gesture_range.value(), gesture_text);
  if (!space_range.has_value())
    return false;

  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  input_method_controller.ReplaceTextAndMoveCaret(
      "", space_range.value(),
      InputMethodController::MoveCaretBehavior::kDoNotMove);
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(space_range->Start(), space_range->Start()));
  return true;
}

StylusWritingGestureSelect::StylusWritingGestureSelect(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect,
    const String& text_alternative,
    const mojom::StylusWritingGestureGranularity granularity)
    : StylusWritingTwoRectGesture(start_rect, end_rect, text_alternative),
      granularity_(granularity) {}

bool StylusWritingGestureSelect::MaybeApplyGesture(LocalFrame* frame) {
  std::optional<PlainTextRange> gesture_range =
      GestureRange(frame, granularity_);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  // Select the text between offsets.
  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  input_method_controller.SetEditableSelectionOffsets(
      gesture_range.value(), /*show_handle=*/true, /*show_context_menu=*/true);
  return true;
}

std::optional<PlainTextRange> StylusWritingGestureSelect::AdjustRange(
    std::optional<PlainTextRange> range,
    InputMethodController& input_method_controller) {
  if (!range.has_value() || range->length() < 2) {
    return range;
  }
  String input_text = input_method_controller.TextInputInfo().value;
  return PlainTextRange(
      range->Start() + IsHTMLSpaceNotLineBreak(input_text[range->Start()]),
      range->End() - IsHTMLSpaceNotLineBreak(input_text[range->End() - 1]));
}

StylusWritingGestureAddText::StylusWritingGestureAddText(
    const gfx::Rect& start_rect,
    const String& text_to_insert,
    const String& text_alternative)
    : StylusWritingGesture(start_rect, text_alternative),
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

StylusWritingGestureSplitOrMerge::StylusWritingGestureSplitOrMerge(
    const gfx::Rect& start_rect,
    const String& text_alternative)
    : StylusWritingGesture(start_rect, text_alternative) {}

bool StylusWritingGestureSplitOrMerge::MaybeApplyGesture(LocalFrame* frame) {
  wtf_size_t gesture_text_index = GetStartTextIndex(frame);
  // When the gesture point is outside the input text range, we get a kNotFound.
  // Return false here to insert the text alternative.
  if (gesture_text_index == kNotFound) {
    return false;
  }

  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  String input_text = input_method_controller.TextInputInfo().value;
  // Gesture cannot be applied if there is no input text.
  if (input_text.empty()) {
    return false;
  }

  // Look for spaces on both side of gesture index.
  wtf_size_t space_start = kNotFound;
  wtf_size_t space_end = kNotFound;
  for (wtf_size_t index = gesture_text_index;
       index < input_text.length() && IsHTMLSpace(input_text[index]); ++index) {
    if (space_start == kNotFound) {
      space_start = index;
    }
    space_end = index + 1;
  }

  for (wtf_size_t index = gesture_text_index;
       index && IsHTMLSpace(input_text[index - 1]); --index) {
    if (space_end == kNotFound) {
      space_end = index;
    }
    space_start = index - 1;
  }

  // No spaces found.
  if (space_start == space_end) {
    // Do not insert space at start of the input text.
    if (gesture_text_index == 0) {
      return false;
    }

    // Insert space at gesture location.
    input_method_controller.SetEditableSelectionOffsets(
        PlainTextRange(gesture_text_index, gesture_text_index));
    frame->GetEditor().InsertText(" ", /* triggering_event = */ nullptr);
    return true;
  }

  // Remove spaces found.
  input_method_controller.ReplaceTextAndMoveCaret(
      "", PlainTextRange(space_start, space_end),
      InputMethodController::MoveCaretBehavior::kDoNotMove);
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(space_start, space_start));
  return true;
}

}  // namespace blink
