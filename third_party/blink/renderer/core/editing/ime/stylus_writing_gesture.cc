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

class StylusWritingTwoPointGesture : public StylusWritingGesture {
 public:
  ~StylusWritingTwoPointGesture() override = default;

 protected:
  StylusWritingTwoPointGesture(const gfx::Point& start_point,
                               const gfx::Point& end_point,
                               const String& text_alternative);

  // Gets the text range in input between the start and end points of this
  // gesture. Returns null if the gesture is not over valid text input.
  absl::optional<PlainTextRange> GestureRange(
      LocalFrame*,
      const mojom::blink::StylusWritingGestureGranularity granularity);

  // End point of the gesure.
  gfx::Point end_point_;
};

class StylusWritingGestureDelete : public StylusWritingTwoPointGesture {
 public:
  ~StylusWritingGestureDelete() override = default;

  StylusWritingGestureDelete(
      const gfx::Point& start_point,
      const gfx::Point& end_point,
      const String& text_alternative,
      const mojom::blink::StylusWritingGestureGranularity granularity);
  bool MaybeApplyGesture(LocalFrame*) override;

 private:
  const mojom::blink::StylusWritingGestureGranularity granularity_;
};

class StylusWritingGestureRemoveSpaces : public StylusWritingTwoPointGesture {
 public:
  ~StylusWritingGestureRemoveSpaces() override = default;

  StylusWritingGestureRemoveSpaces(const gfx::Point& start_point,
                                   const gfx::Point& end_point,
                                   const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;
};

class StylusWritingGestureSelect : public StylusWritingTwoPointGesture {
 public:
  ~StylusWritingGestureSelect() override = default;

  StylusWritingGestureSelect(
      const gfx::Point& start_point,
      const gfx::Point& end_point,
      const String& text_alternative,
      const mojom::StylusWritingGestureGranularity granularity);
  bool MaybeApplyGesture(LocalFrame*) override;

 private:
  const mojom::StylusWritingGestureGranularity granularity_;
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

class StylusWritingGestureSplitOrMerge : public StylusWritingGesture {
 public:
  ~StylusWritingGestureSplitOrMerge() override = default;

  StylusWritingGestureSplitOrMerge(const gfx::Point& start_point,
                                   const String& text_alternative);
  bool MaybeApplyGesture(LocalFrame*) override;
};

std::unique_ptr<StylusWritingGesture> CreateGesture(
    mojom::blink::StylusWritingGestureDataPtr gesture_data) {
  if (!gesture_data) {
    return nullptr;
  }
  gfx::Point start_origin = gesture_data->start_rect.left_center();
  String text_alternative = gesture_data->text_alternative;

  switch (gesture_data->action) {
    case mojom::blink::StylusWritingGestureAction::DELETE_TEXT: {
      if (!gesture_data->end_rect.has_value()) {
        return nullptr;
      }
      return std::make_unique<blink::StylusWritingGestureDelete>(
          start_origin, gesture_data->end_rect->right_center(),
          text_alternative, gesture_data->granularity);
    }
    case mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT: {
      return std::make_unique<blink::StylusWritingGestureAddText>(
          start_origin, gesture_data->text_to_insert, text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::REMOVE_SPACES: {
      if (!gesture_data->end_rect.has_value()) {
        return nullptr;
      }
      return std::make_unique<blink::StylusWritingGestureRemoveSpaces>(
          start_origin, gesture_data->end_rect->right_center(),
          text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE: {
      return std::make_unique<blink::StylusWritingGestureSplitOrMerge>(
          start_origin, text_alternative);
    }
    case mojom::blink::StylusWritingGestureAction::SELECT_TEXT: {
      if (!gesture_data->end_rect.has_value()) {
        return nullptr;
      }
      return std::make_unique<blink::StylusWritingGestureSelect>(
          start_origin, gesture_data->end_rect->right_center(),
          text_alternative, gesture_data->granularity);
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

PlainTextRange ExpandWithWordGranularity(
    EphemeralRange ephemeral_range,
    Element* const root_editable_element,
    InputMethodController& input_method_controller) {
  SelectionInDOMTree expanded_selection = ExpandWithGranularity(
      SelectionInDOMTree::Builder().SetBaseAndExtent(ephemeral_range).Build(),
      TextGranularity::kWord, WordInclusion::kMiddle);
  PlainTextRange expanded_range = PlainTextRange::Create(
      *root_editable_element, expanded_selection.ComputeRange());
  String input_text = input_method_controller.TextInputInfo().value;
  if (expanded_range.length() > 2 &&
      IsHTMLSpace(input_text[expanded_range.Start()]) &&
      IsHTMLSpace(input_text[expanded_range.End() - 1])) {
    // Special case, we don't want to delete spaces both sides of the
    // selection as that will join words together.
    return PlainTextRange(expanded_range.Start() + 1, expanded_range.End());
  }
  return expanded_range;
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
  if (gesture == nullptr || !gesture->MaybeApplyGesture(local_frame)) {
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
    LocalFrame* local_frame,
    const mojom::blink::StylusWritingGestureGranularity granularity) {
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
  switch (granularity) {
    case mojom::blink::StylusWritingGestureGranularity::CHARACTER:
      return gesture_range;
    case mojom::blink::StylusWritingGestureGranularity::WORD:
      return ExpandWithWordGranularity(ephemeral_range, root_editable_element,
                                       local_frame->GetInputMethodController());
    default:
      return absl::nullopt;
  }
}

StylusWritingGestureDelete::StylusWritingGestureDelete(
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    const String& text_alternative,
    const mojom::blink::StylusWritingGestureGranularity granularity)
    : StylusWritingTwoPointGesture(start_point, end_point, text_alternative),
      granularity_(granularity) {}

bool StylusWritingGestureDelete::MaybeApplyGesture(LocalFrame* frame) {
  absl::optional<PlainTextRange> gesture_range =
      GestureRange(frame, granularity_);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  // Delete the text between offsets and set cursor.
  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
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
  absl::optional<PlainTextRange> gesture_range = GestureRange(
      frame, mojom::blink::StylusWritingGestureGranularity::CHARACTER);
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

StylusWritingGestureSelect::StylusWritingGestureSelect(
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    const String& text_alternative,
    const mojom::StylusWritingGestureGranularity granularity)
    : StylusWritingTwoPointGesture(start_point, end_point, text_alternative),
      granularity_(granularity) {}

bool StylusWritingGestureSelect::MaybeApplyGesture(LocalFrame* frame) {
  absl::optional<PlainTextRange> gesture_range =
      GestureRange(frame, granularity_);
  if (!gesture_range.has_value()) {
    // Invalid gesture, return false to insert the alternative text.
    return false;
  }

  // Select the text between offsets.
  InputMethodController& input_method_controller =
      frame->GetInputMethodController();
  DCHECK_EQ(granularity_, mojom::StylusWritingGestureGranularity::CHARACTER);
  input_method_controller.SetEditableSelectionOffsets(gesture_range.value());
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

StylusWritingGestureSplitOrMerge::StylusWritingGestureSplitOrMerge(
    const gfx::Point& start_point,
    const String& text_alternative)
    : StylusWritingGesture(start_point, text_alternative) {}

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
  input_method_controller.ReplaceText("",
                                      PlainTextRange(space_start, space_end));
  input_method_controller.SetEditableSelectionOffsets(
      PlainTextRange(space_start, space_start));
  return true;
}

}  // namespace blink
