// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_module.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "pdf/draw_utils/page_boundary_intersect.h"
#include "pdf/input_utils.h"
#include "pdf/message_util.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_caret.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_cursor.h"
#include "pdf/pdf_ink_metrics_handler.h"
#include "pdf/pdf_ink_module_client.h"
#include "pdf/pdf_ink_transform.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/common/input/web_touch_point.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/intersects.h"
#include "third_party/ink/src/ink/geometry/partitioned_mesh.h"
#include "third_party/ink/src/ink/geometry/rect.h"
#include "third_party/ink/src/ink/rendering/skia/native/skia_renderer.h"
#include "third_party/ink/src/ink/strokes/in_progress_stroke.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

namespace {

constexpr ink::AffineTransform kIdentityTransform;

constexpr SkColor kEraserColor = SK_ColorWHITE;
constexpr int kEraserSize = 3;

// `is_ink` represents the Ink thumbnail when true, and the PDF thumbnail when
// false.
base::Value::Dict CreateUpdateThumbnailMessage(
    int page_index,
    bool is_ink,
    std::vector<uint8_t> image_data,
    const gfx::Size& thumbnail_size) {
  return base::Value::Dict()
      .Set("type", "updateInk2Thumbnail")
      .Set("pageNumber", page_index + 1)
      .Set("isInk", is_ink)
      .Set("imageData", std::move(image_data))
      .Set("width", thumbnail_size.width())
      .Set("height", thumbnail_size.height());
}

ink::StrokeInput::ToolType GetToolTypeFromTouchEvent(
    const blink::WebTouchEvent& event) {
  // Assumes the caller already handled multi-touch events.
  CHECK_EQ(event.touches_length, 1u);
  return event.touches[0].pointer_type ==
                 blink::WebPointerProperties::PointerType::kPen
             ? ink::StrokeInput::ToolType::kStylus
             : ink::StrokeInput::ToolType::kTouch;
}

PdfInkBrush CreateDefaultHighlighterBrush() {
  return PdfInkBrush(PdfInkBrush::Type::kHighlighter,
                     SkColorSetRGB(0xF2, 0x8B, 0x82),
                     /*size=*/8.0f);
}

PdfInkBrush CreateDefaultPenBrush() {
  return PdfInkBrush(PdfInkBrush::Type::kPen, SK_ColorBLACK, /*size=*/3.0f);
}

// Check if `color` is a valid color value within range.
void CheckColorIsWithinRange(int color) {
  CHECK_GE(color, 0);
  CHECK_LE(color, 255);
}

ink::Rect GetEraserRect(const gfx::PointF& center) {
  return ink::Rect::FromTwoPoints(
      {center.x() - kEraserSize, center.y() - kEraserSize},
      {center.x() + kEraserSize, center.y() + kEraserSize});
}

SkRect GetDrawPageClipRect(const gfx::Rect& content_rect,
                           const gfx::Vector2dF& origin_offset) {
  gfx::RectF clip_rect(content_rect);
  clip_rect.Offset(origin_offset);
  return gfx::RectFToSkRect(clip_rect);
}

blink::WebMouseEvent GenerateLeftMouseUpEvent(const gfx::PointF& position,
                                              base::TimeTicks timestamp) {
  return blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseUp,
      /*position=*/position,
      /*global_position=*/position, blink::WebPointerProperties::Button::kLeft,
      /*click_count_param=*/1, blink::WebInputEvent::Modifiers::kNoModifiers,
      timestamp);
}

bool IsPrintOrSaveKeyboardEvent(const blink::WebKeyboardEvent& event) {
  const bool is_ctrl =
      !!(event.GetModifiers() & blink::WebInputEvent::Modifiers::kControlKey);
  return is_ctrl && (event.windows_key_code == ui::KeyboardCode::VKEY_P ||
                     event.windows_key_code == ui::KeyboardCode::VKEY_S);
}

}  // namespace

PdfInkModule::PdfInkModule(PdfInkModuleClient& client)
    : client_(client),
      highlighter_brush_(CreateDefaultHighlighterBrush()),
      pen_brush_(CreateDefaultPenBrush()) {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInk2));
  CHECK(is_drawing_stroke());

  // Default to a pen brush.
  drawing_stroke_state().brush_type = PdfInkBrush::Type::kPen;
}

PdfInkModule::~PdfInkModule() = default;

bool PdfInkModule::ShouldBlockTextSelectionChanged() {
  return features::kPdfInk2TextHighlighting.Get() && is_text_highlighting();
}

bool PdfInkModule::HasInputsToDraw() const {
  if (mode_ != InkAnnotationMode::kDraw || is_erasing_stroke()) {
    return false;
  }

  if (is_text_highlighting()) {
    return !text_highlight_state().highlight_strokes.empty();
  }

  CHECK(is_drawing_stroke());
  return !drawing_stroke_state().inputs.empty();
}

void PdfInkModule::Draw(SkCanvas& canvas) {
  ink::SkiaRenderer skia_renderer;

  if (is_text_highlighting()) {
    const auto& highlight_strokes = text_highlight_state().highlight_strokes;
    CHECK(!highlight_strokes.empty());

    for (const auto& [page_index, strokes] : highlight_strokes) {
      SkAutoCanvasRestore save_restore(&canvas, /*doSave=*/true);
      const auto [transform, clip_rect] = GetTransformAndClipRect(page_index);
      canvas.clipRect(clip_rect);
      for (const auto& stroke : strokes) {
        auto status = skia_renderer.Draw(nullptr, stroke, transform, canvas);
        CHECK(status.ok());
      }
    }
    return;
  }

  CHECK(is_drawing_stroke());

  auto in_progress_stroke = CreateInProgressStrokeSegmentsFromInputs();
  CHECK(!in_progress_stroke.empty());

  SkAutoCanvasRestore save_restore(&canvas, /*doSave=*/true);
  const auto [transform, clip_rect] =
      GetTransformAndClipRect(drawing_stroke_state().page_index);
  canvas.clipRect(clip_rect);
  for (const auto& segment : in_progress_stroke) {
    auto status = skia_renderer.Draw(nullptr, segment, transform, canvas);
    CHECK(status.ok());
  }
}

PdfInkModule::TransformAndClipRect PdfInkModule::GetTransformAndClipRect(
    int page_index) {
  const gfx::Vector2dF origin_offset = client_->GetViewportOriginOffset();
  const PageOrientation rotation = client_->GetOrientation();

  const gfx::Rect content_rect = client_->GetPageContentsRect(page_index);
  const gfx::SizeF page_size_in_points =
      client_->GetPageSizeInPoints(page_index);
  ink::AffineTransform transform = GetInkRenderTransform(
      origin_offset, rotation, content_rect, page_size_in_points);

  return {transform, GetDrawPageClipRect(content_rect, origin_offset)};
}

void PdfInkModule::GenerateAndSendInkThumbnail(
    int page_index,
    const gfx::Size& thumbnail_size) {
  CHECK(!thumbnail_size.IsEmpty());

  auto info = SkImageInfo::Make(thumbnail_size.width(), thumbnail_size.height(),
                                kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  const size_t alloc_size = info.computeMinByteSize();
  CHECK(!SkImageInfo::ByteSizeOverflowed(alloc_size));
  std::vector<uint8_t> image_data(alloc_size);

  SkBitmap sk_bitmap;
  sk_bitmap.installPixels(info, image_data.data(), info.minRowBytes());
  SkCanvas canvas(sk_bitmap);
  if (!DrawThumbnail(canvas, page_index)) {
    return;
  }

  client_->PostMessage(CreateUpdateThumbnailMessage(
      page_index,
      /*is_ink=*/true, std::move(image_data), thumbnail_size));
}

void PdfInkModule::GenerateAndSendInkThumbnailInternal(int page_index) {
  return GenerateAndSendInkThumbnail(page_index,
                                     client_->GetThumbnailSize(page_index));
}

bool PdfInkModule::DrawThumbnail(SkCanvas& canvas, int page_index) {
  auto it = strokes_.find(page_index);
  if (it == strokes_.end() || it->second.empty()) {
    return false;
  }

  const ink::AffineTransform transform = GetInkThumbnailTransform(
      gfx::SkISizeToSize(canvas.imageInfo().dimensions()),
      client_->GetOrientation(), client_->GetPageContentsRect(page_index),
      client_->GetZoom());

  ink::SkiaRenderer skia_renderer;
  for (const FinishedStrokeState& finished_stroke : it->second) {
    if (!finished_stroke.should_draw) {
      continue;
    }

    auto status =
        skia_renderer.Draw(nullptr, finished_stroke.stroke, transform, canvas);
    CHECK(status.ok());
  }

  // No need to draw in-progress strokes, since DrawThumbnail() only gets called
  // after the in-progress strokes finish.
  return true;
}

void PdfInkModule::RequestThumbnailUpdates(
    const base::flat_set<int>& ink_updates,
    const base::flat_set<int>& pdf_updates) {
  for (int page_index : ink_updates) {
    GenerateAndSendInkThumbnailInternal(page_index);
  }
  for (int page_index : pdf_updates) {
    client_->RequestThumbnail(
        page_index, base::BindOnce(&PdfInkModule::OnGotThumbnail,
                                   weak_factory_.GetWeakPtr(), page_index));
  }
}

void PdfInkModule::OnGotThumbnail(int page_index, Thumbnail thumbnail) {
  client_->PostMessage(CreateUpdateThumbnailMessage(
      page_index,
      /*is_ink=*/false, thumbnail.TakeData(), thumbnail.image_size()));
}

bool PdfInkModule::HandleInputEvent(const blink::WebInputEvent& event) {
  if (mode_ != InkAnnotationMode::kDraw) {
    return false;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kKeyDown:
      // Blink mostly sends `kRawKeyDown`, but sometimes generates `kKeyDown`,
      // such as when tabbing between frames.
    case blink::WebInputEvent::Type::kRawKeyDown:
      return OnKeyDown(static_cast<const blink::WebKeyboardEvent&>(event));
    case blink::WebInputEvent::Type::kMouseDown:
      return OnMouseDown(static_cast<const blink::WebMouseEvent&>(event));
    case blink::WebInputEvent::Type::kMouseUp:
      return OnMouseUp(static_cast<const blink::WebMouseEvent&>(event));
    case blink::WebInputEvent::Type::kMouseMove:
      return OnMouseMove(static_cast<const blink::WebMouseEvent&>(event));
    // Touch and pen input events are blink::WebTouchEvent instances.
    case blink::WebInputEvent::Type::kTouchStart:
      return OnTouchStart(static_cast<const blink::WebTouchEvent&>(event));
    case blink::WebInputEvent::Type::kTouchEnd:
      return OnTouchEnd(static_cast<const blink::WebTouchEvent&>(event));
    case blink::WebInputEvent::Type::kTouchMove:
      return OnTouchMove(static_cast<const blink::WebTouchEvent&>(event));
    default:
      return false;
  }
}

bool PdfInkModule::OnMessage(const base::Value::Dict& message) {
  using MessageHandler = void (PdfInkModule::*)(const base::Value::Dict&);

  static constexpr auto kMessageHandlers =
      base::MakeFixedFlatMap<std::string_view, MessageHandler>({
          {"annotationRedo", &PdfInkModule::HandleAnnotationRedoMessage},
          {"annotationUndo", &PdfInkModule::HandleAnnotationUndoMessage},
          {"finishTextAnnotation",
           &PdfInkModule::HandleFinishTextAnnotationMessage},
          {"getAllTextAnnotations",
           &PdfInkModule::HandleGetAllTextAnnotationsMessage},
          {"getAnnotationBrush",
           &PdfInkModule::HandleGetAnnotationBrushMessage},
          {"setAnnotationBrush",
           &PdfInkModule::HandleSetAnnotationBrushMessage},
          {"setAnnotationMode", &PdfInkModule::HandleSetAnnotationModeMessage},
          {"startTextAnnotation",
           &PdfInkModule::HandleStartTextAnnotationMessage},
      });

  auto it = kMessageHandlers.find(*message.FindString("type"));
  if (it == kMessageHandlers.end()) {
    return false;
  }

  MessageHandler handler = it->second;
  (this->*handler)(message);
  return true;
}

void PdfInkModule::OnGeometryChanged() {
  // If the highlighter tool is selected, and zooming moves the cursor onto
  // text, the cursor should be an I-beam, but it will instead be the drawing
  // cursor until a mousemove event occurs. There is not a way to get the new
  // mouse position on geometry change.
  MaybeSetCursor();
}

const PdfInkBrush* PdfInkModule::GetPdfInkBrushForTesting() const {
  return is_drawing_stroke() ? &GetDrawingBrush() : nullptr;
}

bool PdfInkModule::OnKeyDown(const blink::WebKeyboardEvent& event) {
  if (!features::kPdfInk2TextHighlighting.Get()) {
    return false;
  }

  // Return false if not starting or continuing a text highlight.
  if (is_erasing_stroke() ||
      (is_drawing_stroke() &&
       drawing_stroke_state().brush_type != PdfInkBrush::Type::kHighlighter)) {
    return false;
  }

  PdfCaret* caret = client_->GetPdfCaret();
  if (!caret || !caret->enabled()) {
    return false;
  }

  if (!caret->WillHandleKeyDownEvent(event)) {
    if (is_text_highlighting() && IsPrintOrSaveKeyboardEvent(event)) {
      const EventDetails& event_details =
          text_highlight_state().input_last_event;
      FinishTextHighlight(event_details.position, /*is_multi_click=*/false,
                          event_details.tool_type);
      // Return false so that the event propagates to the handler that actually
      // starts the print/save.
    }
    return false;
  }

  const bool shift_key =
      !!(event.GetModifiers() & blink::WebInputEvent::Modifiers::kShiftKey);
  if (!shift_key && is_text_highlighting()) {
    // End the text highlight. `FinishTextHighlight()` must be called first,
    // otherwise `PdfCaret::OnKeyDown()` could clear the text selection.
    const EventDetails& event_details = text_highlight_state().input_last_event;
    FinishTextHighlight(event_details.position, /*is_multi_click=*/false,
                        event_details.tool_type);
  }

  CHECK(caret->OnKeyDown(event));

  if (!shift_key) {
    return true;
  }

  // Handle shift + arrow key events.
  //
  // Start a text selection if necessary.
  if (!is_text_highlighting()) {
    client_->StrokeStarted();
    current_tool_state_.emplace<TextHighlightState>();
    text_highlight_state().initiated_by_keyboard = true;
    std::optional<PdfInkUndoRedoModel::DiscardedDrawCommands> discards =
        undo_redo_model_.StartDraw();
    CHECK(discards.has_value());
    ApplyUndoRedoDiscards(discards.value());
  }

  text_highlight_state().highlight_strokes = GetTextSelectionAsStrokes();
  return true;
}

bool PdfInkModule::OnMouseDown(const blink::WebMouseEvent& event) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  gfx::PointF position = normalized_event.PositionInWidget();
  if (is_erasing_stroke()) {
    return StartEraseStroke(position, ink::StrokeInput::ToolType::kMouse);
  }

  if (is_drawing_stroke()) {
    MaybeFinishStrokeForMissingMouseUpEvent();
  } else if (features::kPdfInk2TextHighlighting.Get() &&
             is_text_highlighting()) {
    const EventDetails& event_details = text_highlight_state().input_last_event;
    FinishTextHighlight(event_details.position,
                        /*is_multi_click=*/false, event_details.tool_type);
  }

  if (IsHighlightingTextAtPosition(position)) {
    return StartTextHighlight(position, event.ClickCount(),
                              ink::StrokeInput::ToolType::kMouse);
  }

  return StartStroke(position, event.TimeStamp(),
                     ink::StrokeInput::ToolType::kMouse,
                     /*properties=*/nullptr);
}

bool PdfInkModule::OnMouseUp(const blink::WebMouseEvent& event) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  if (event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  gfx::PointF position = event.PositionInWidget();
  if (features::kPdfInk2TextHighlighting.Get() && is_text_highlighting()) {
    return FinishTextHighlight(position, /*is_multi_click=*/false,
                               ink::StrokeInput::ToolType::kMouse);
  }

  return is_drawing_stroke()
             ? FinishStroke(position, event.TimeStamp(),
                            ink::StrokeInput::ToolType::kMouse,
                            /*properties=*/nullptr)
             : FinishEraseStroke(position, ink::StrokeInput::ToolType::kMouse);
}

bool PdfInkModule::OnMouseMove(const blink::WebMouseEvent& event) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  // Before the multi-click text selection timer fired, the mouse moved to a new
  // position, so the click count can no longer increment. Fire the timer
  // immediately.
  if (features::kPdfInk2TextHighlighting.Get() &&
      text_selection_click_timer_.IsRunning()) {
    text_selection_click_timer_.FireNow();
  }

  gfx::PointF position = event.PositionInWidget();

  bool still_interacting_with_ink =
      event.GetModifiers() & blink::WebInputEvent::kLeftButtonDown;
  if (still_interacting_with_ink) {
    if (features::kPdfInk2TextHighlighting.Get() && is_text_highlighting()) {
      return ContinueTextHighlight(position);
    }

    return is_drawing_stroke()
               ? ContinueStroke(position, event.TimeStamp(),
                                ink::StrokeInput::ToolType::kMouse,
                                /*properties=*/nullptr)
               : ContinueEraseStroke(position,
                                     ink::StrokeInput::ToolType::kMouse);
  }

  // Some other view consumed the input events sometime after the stroke was
  // started, and the input end event went missing for PdfInkModule.  Notice
  // that now, and compensate by synthesizing a mouse-up input event at the
  // last known input position.  Intentionally do not use `position`.
  if (is_drawing_stroke()) {
    MaybeSetCursorOnMouseMove(position);
    DrawingStrokeState& state = drawing_stroke_state();
    if (!state.input_last_event.has_value()) {
      // Ignore when not drawing.
      return false;
    }

    const EventDetails& input_last_event = state.input_last_event.value();
    return OnMouseUp(GenerateLeftMouseUpEvent(input_last_event.position,
                                              input_last_event.timestamp));
  }

  if (features::kPdfInk2TextHighlighting.Get() && is_text_highlighting()) {
    // Text highlighting is not sensitive to particular timestamps, just use
    // current time.
    return OnMouseUp(GenerateLeftMouseUpEvent(
        text_highlight_state().input_last_event.position,
        base::TimeTicks::Now()));
  }

  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  if (!state.input_last_event_position.has_value()) {
    // Ignore when not erasing.
    CHECK(!state.erasing);
    return false;
  }

  // Erasing is not sensitive to particular timestamps, just use current time.
  return OnMouseUp(GenerateLeftMouseUpEvent(
      state.input_last_event_position.value(), base::TimeTicks::Now()));
}

bool PdfInkModule::OnTouchStart(const blink::WebTouchEvent& event) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  if (event.touches_length != 1) {
    return false;
  }

  ink::StrokeInput::ToolType tool_type = GetToolTypeFromTouchEvent(event);
  MaybeRecordPenInput(tool_type);
  if (ShouldIgnoreTouchInput(tool_type)) {
    return false;
  }

  const blink::WebTouchPoint& touch_point = event.touches[0];
  gfx::PointF position = touch_point.PositionInWidget();
  if (is_erasing_stroke()) {
    return StartEraseStroke(position, tool_type);
  }

  if (is_drawing_stroke()) {
    MaybeFinishStrokeForMissingMouseUpEvent();
  } else if (features::kPdfInk2TextHighlighting.Get() &&
             is_text_highlighting()) {
    const EventDetails& event_details = text_highlight_state().input_last_event;
    FinishTextHighlight(event_details.position,
                        /*is_multi_click=*/false, event_details.tool_type);
  }

  if (IsHighlightingTextAtPosition(position)) {
    // Multi-click text selection for touch is not supported.
    return StartTextHighlight(position, /*click_count=*/1, tool_type);
  }

  return StartStroke(position, event.TimeStamp(), tool_type, &touch_point);
}

bool PdfInkModule::OnTouchEnd(const blink::WebTouchEvent& event) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  if (event.touches_length != 1) {
    return false;
  }

  ink::StrokeInput::ToolType tool_type = GetToolTypeFromTouchEvent(event);
  MaybeRecordPenInput(tool_type);
  if (ShouldIgnoreTouchInput(tool_type)) {
    return false;
  }

  const blink::WebTouchPoint& touch_point = event.touches[0];
  gfx::PointF position = touch_point.PositionInWidget();
  if (features::kPdfInk2TextHighlighting.Get() && is_text_highlighting()) {
    return FinishTextHighlight(position, /*is_multi_click=*/false, tool_type);
  }

  if (is_drawing_stroke()) {
    return FinishStroke(position, event.TimeStamp(), tool_type, &touch_point);
  }
  return FinishEraseStroke(position, tool_type);
}

bool PdfInkModule::OnTouchMove(const blink::WebTouchEvent& event) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  if (event.touches_length != 1) {
    return false;
  }

  ink::StrokeInput::ToolType tool_type = GetToolTypeFromTouchEvent(event);
  MaybeRecordPenInput(tool_type);
  if (ShouldIgnoreTouchInput(tool_type)) {
    return false;
  }

  const blink::WebTouchPoint& touch_point = event.touches[0];
  gfx::PointF position = touch_point.PositionInWidget();
  if (features::kPdfInk2TextHighlighting.Get() && is_text_highlighting()) {
    return ContinueTextHighlight(position);
  }

  if (is_drawing_stroke()) {
    return ContinueStroke(position, event.TimeStamp(), tool_type, &touch_point);
  }
  return ContinueEraseStroke(position, tool_type);
}

void PdfInkModule::MaybeFinishStrokeForMissingMouseUpEvent() {
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.start_time.has_value()) {
    return;
  }

  CHECK(state.input_last_event.has_value());
  const EventDetails& input_last_event = state.input_last_event.value();
  bool mouse_up_result = OnMouseUp(GenerateLeftMouseUpEvent(
      input_last_event.position, input_last_event.timestamp));
  CHECK(mouse_up_result);
}

bool PdfInkModule::StartStroke(const gfx::PointF& position,
                               base::TimeTicks timestamp,
                               ink::StrokeInput::ToolType tool_type,
                               const blink::WebPointerProperties* properties) {
  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do not draw when not on a page.
    return false;
  }

  client_->StrokeStarted();

  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();

  gfx::PointF page_position =
      GetEventToCanonicalTransformForPage(page_index).MapPoint(position);

  CHECK(!state.start_time.has_value());
  state.start_time = timestamp;
  state.page_index = page_index;

  // Start of the first segment of a stroke.
  ink::StrokeInputBatch segment;
  auto result = segment.Append(CreateInkStrokeInputWithProperties(
      tool_type, page_position, /*elapsed_time=*/base::TimeDelta(),
      properties));
  CHECK(result.ok());
  state.inputs.push_back(std::move(segment));

  // Invalidate area around this one point.
  client_->Invalidate(GetDrawingBrush().GetInvalidateArea(position, position));

  std::optional<PdfInkUndoRedoModel::DiscardedDrawCommands> discards =
      undo_redo_model_.StartDraw();
  CHECK(discards.has_value());
  ApplyUndoRedoDiscards(discards.value());

  // Remember this location and timestamp to support invalidating all of the
  // area between this location and the next position, and to possibly
  // compensate for missed input events.
  CHECK(!state.input_last_event.has_value());
  state.input_last_event = EventDetails{position, timestamp, tool_type};

  return true;
}

bool PdfInkModule::ContinueStroke(
    const gfx::PointF& position,
    base::TimeTicks timestamp,
    ink::StrokeInput::ToolType tool_type,
    const blink::WebPointerProperties* properties) {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.start_time.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  CHECK(state.input_last_event.has_value());
  const gfx::PointF last_position = state.input_last_event.value().position;
  if (position == last_position) {
    // Since the position did not change, do nothing.
    return true;
  }

  if (state.input_last_event.value().tool_type != tool_type) {
    // Ignore if the user is simultaneously using a different input type.
    return true;
  }

  const int page_index = client_->VisiblePageIndexFromPoint(position);
  const int last_page_index = client_->VisiblePageIndexFromPoint(last_position);
  if (page_index != state.page_index && last_page_index != state.page_index) {
    // If `position` is outside the page, and so was `last_position`, then just
    // update `last_input_event` and treat the event as handled.
    state.input_last_event = EventDetails{position, timestamp, tool_type};
    return true;
  }

  CHECK_GE(state.page_index, 0);
  if (page_index != state.page_index) {
    // `position` is outside the page, and `last_position` is inside the page.
    CHECK_EQ(last_page_index, state.page_index);
    const gfx::PointF boundary_position = CalculatePageBoundaryIntersectPoint(
        client_->GetPageContentsRect(state.page_index), last_position,
        position);
    if (boundary_position != last_position) {
      // Record the last point before leaving the page, if `last_position` was
      // not already on the page boundary.
      if (RecordStrokePosition(boundary_position, timestamp, tool_type,
                               properties)) {
        client_->Invalidate(GetDrawingBrush().GetInvalidateArea(
            last_position, boundary_position));
      }
    }

    // Remember `position` and `timestamp` for use in the next event and treat
    // event as handled.
    state.input_last_event = EventDetails{position, timestamp, tool_type};
    return true;
  }

  gfx::PointF invalidation_position = last_position;
  if (last_page_index != state.page_index) {
    // If the stroke left the page and is now re-entering, then start a new
    // segment.
    CHECK(!state.inputs.back().IsEmpty());
    state.inputs.push_back(ink::StrokeInputBatch());
    const gfx::PointF boundary_position = CalculatePageBoundaryIntersectPoint(
        client_->GetPageContentsRect(state.page_index), position,
        last_position);
    if (boundary_position != position) {
      // Record the first point after entering the page.
      if (RecordStrokePosition(boundary_position, timestamp, tool_type,
                               properties)) {
        invalidation_position = boundary_position;
      }
    }
  }

  if (RecordStrokePosition(position, timestamp, tool_type, properties)) {
    // Invalidate area covering a straight line between this position and the
    // previous one.
    client_->Invalidate(
        GetDrawingBrush().GetInvalidateArea(position, invalidation_position));
  }

  // Remember `position` and `timestamp` for use in the next event.
  state.input_last_event = EventDetails{position, timestamp, tool_type};

  return true;
}

bool PdfInkModule::FinishStroke(const gfx::PointF& position,
                                base::TimeTicks timestamp,
                                ink::StrokeInput::ToolType tool_type,
                                const blink::WebPointerProperties* properties) {
  // Process `position` as though it was the last point of movement first,
  // before moving on to various bookkeeping tasks.
  if (!ContinueStroke(position, timestamp, tool_type, properties)) {
    return false;
  }

  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  auto in_progress_stroke_segments = CreateInProgressStrokeSegmentsFromInputs();
  if (!in_progress_stroke_segments.empty()) {
    CHECK_GE(state.page_index, 0);
    ink::Envelope invalidate_envelope;
    for (const auto& segment : in_progress_stroke_segments) {
      InkStrokeId id = stroke_id_generator_.GetIdAndAdvance();
      ink::Stroke stroke = segment.CopyToStroke();
      client_->StrokeAdded(state.page_index, id, stroke);
      invalidate_envelope.Add(stroke.GetShape().Bounds());
      strokes_[state.page_index].push_back(
          FinishedStrokeState(std::move(stroke), id));
      bool undo_redo_success = undo_redo_model_.Draw(id);
      CHECK(undo_redo_success);
    }

    client_->Invalidate(CanonicalInkEnvelopeToInvalidationScreenRect(
        invalidate_envelope,
        GetCanonicalToEventTransformForPage(state.page_index)));
  }

  client_->StrokeFinished(/*modified=*/true);
  GenerateAndSendInkThumbnailInternal(state.page_index);

  bool undo_redo_success = undo_redo_model_.FinishDraw();
  CHECK(undo_redo_success);

  ReportDrawStroke(state.brush_type, GetDrawingBrush().ink_brush(), tool_type);

  // Reset `state` now that the stroke operation is done.
  state.inputs.clear();
  state.start_time = std::nullopt;
  state.page_index = -1;
  state.input_last_event.reset();

  bool set_drawing_brush = MaybeSetDrawingBrush();
  if (IsHighlightingTextAtPosition(position)) {
    client_->UpdateInkCursor(ui::mojom::CursorType::kIBeam);
  } else if (set_drawing_brush) {
    MaybeSetCursor();
  }

  return true;
}

bool PdfInkModule::StartEraseStroke(const gfx::PointF& position,
                                    ink::StrokeInput::ToolType tool_type) {
  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do not erase when not on a page.
    return false;
  }

  client_->StrokeStarted();

  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  CHECK(!state.erasing);
  state.erasing = true;

  std::optional<PdfInkUndoRedoModel::DiscardedDrawCommands> discards =
      undo_redo_model_.StartErase();
  CHECK(discards.has_value());
  ApplyUndoRedoDiscards(discards.value());

  EraseHelper(position, page_index);

  // Remember this position to possibly compensate for missed input events.
  CHECK(!state.input_last_event_position.has_value());
  state.input_last_event_position = position;
  state.tool_type = tool_type;

  return true;
}

bool PdfInkModule::ContinueEraseStroke(const gfx::PointF& position,
                                       ink::StrokeInput::ToolType tool_type) {
  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  if (!state.erasing) {
    return false;
  }

  state.tool_type = tool_type;

  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do nothing when the eraser tool is in use, but the event position is
    // off-page. Treat the event as handled to be consistent with
    // ContinueStroke(), and so that nothing else attempts to handle this event.
    // Remember this position for possible use in the next event.
    state.input_last_event_position = position;
    return true;
  }

  EraseHelper(position, page_index);

  // Remember this position for possible use in the next event.
  state.input_last_event_position = position;

  return true;
}

bool PdfInkModule::FinishEraseStroke(const gfx::PointF& position,
                                     ink::StrokeInput::ToolType tool_type) {
  // Process `position` as though it was the last point of movement first,
  // before moving on to various bookkeeping tasks.
  if (!ContinueEraseStroke(position, tool_type)) {
    return false;
  }

  bool undo_redo_success = undo_redo_model_.FinishErase();
  CHECK(undo_redo_success);

  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  const bool modified =
      !state.page_indices_with_stroke_erasures.empty() ||
      !state.page_indices_with_partitioned_mesh_erasures.empty();
  if (modified) {
    RequestThumbnailUpdates(
        /*ink_updates=*/state.page_indices_with_stroke_erasures,
        /*pdf_updates=*/state.page_indices_with_partitioned_mesh_erasures);
    ReportEraseStroke(tool_type);
  }

  client_->StrokeFinished(modified);

  // Reset `state` now that the erase operation is done.
  state.erasing = false;
  state.page_indices_with_stroke_erasures.clear();
  state.page_indices_with_partitioned_mesh_erasures.clear();
  state.input_last_event_position.reset();
  state.tool_type = ink::StrokeInput::ToolType::kUnknown;

  if (MaybeSetDrawingBrush()) {
    MaybeSetCursor();
  }

  return true;
}

void PdfInkModule::EraseHelper(const gfx::PointF& position, int page_index) {
  CHECK_GE(page_index, 0);

  const gfx::PointF canonical_position =
      GetEventToCanonicalTransformForPage(page_index).MapPoint(position);
  const ink::Rect eraser_rect = GetEraserRect(canonical_position);
  ink::Envelope invalidate_envelope;

  bool erased_stroke = false;
  if (auto stroke_it = strokes_.find(page_index); stroke_it != strokes_.end()) {
    for (auto& stroke : stroke_it->second) {
      if (!stroke.should_draw) {
        // Already erased.
        continue;
      }

      // No transform needed, as `eraser_rect` is already using transformed
      // coordinates from `canonical_position`.
      const ink::PartitionedMesh& shape = stroke.stroke.GetShape();
      if (!ink::Intersects(eraser_rect, shape, kIdentityTransform)) {
        continue;
      }

      stroke.should_draw = false;
      client_->UpdateStrokeActive(page_index, stroke.id, /*active=*/false);

      invalidate_envelope.Add(shape.Bounds());
      erased_stroke = true;

      bool undo_redo_success = undo_redo_model_.EraseStroke(stroke.id);
      CHECK(undo_redo_success);
    }
  }

  bool erased_partitioned_mesh = false;
  if (auto shape_it = loaded_v2_shapes_.find(page_index);
      shape_it != loaded_v2_shapes_.end()) {
    for (auto& shape_state : shape_it->second) {
      if (!shape_state.should_draw) {
        // Already erased.
        continue;
      }

      // No transform needed, as `eraser_rect` is already using transformed
      // coordinates from `canonical_position`.
      if (!ink::Intersects(eraser_rect, shape_state.shape,
                           kIdentityTransform)) {
        continue;
      }

      shape_state.should_draw = false;
      client_->UpdateShapeActive(page_index, shape_state.id, /*active=*/false);

      invalidate_envelope.Add(shape_state.shape.Bounds());
      erased_partitioned_mesh = true;

      bool undo_redo_success = undo_redo_model_.EraseShape(shape_state.id);
      CHECK(undo_redo_success);
    }
  }

  if (invalidate_envelope.IsEmpty()) {
    CHECK(!erased_stroke);
    CHECK(!erased_partitioned_mesh);
    return;
  }

  // If `invalidate_envelope` isn't empty, then something got erased.
  client_->Invalidate(CanonicalInkEnvelopeToInvalidationScreenRect(
      invalidate_envelope, GetCanonicalToEventTransformForPage(page_index)));

  CHECK(erased_stroke || erased_partitioned_mesh);
  EraserState& state = erasing_stroke_state();
  if (erased_stroke) {
    state.page_indices_with_stroke_erasures.insert(page_index);
  }
  if (erased_partitioned_mesh) {
    state.page_indices_with_partitioned_mesh_erasures.insert(page_index);
  }
}

bool PdfInkModule::StartTextHighlight(const gfx::PointF& position,
                                      int click_count,
                                      ink::StrokeInput::ToolType tool_type) {
  client_->StrokeStarted();

  current_tool_state_.emplace<TextHighlightState>();
  // Text highlighting does not need the timestamp of the event.
  text_highlight_state().input_last_event =
      EventDetails{.position = position, .tool_type = tool_type};

  bool is_double_click = click_count == 2;
  bool is_triple_click = click_count == 3;
  if (is_double_click) {
    StartTextSelectionMultiClickTimer(tool_type);
  } else if (is_triple_click) {
    StopTextSelectionMultiClickTimer();
    // Clicking the same text position two times will select the word. An
    // additional third click will select the line. `StartTextHighlight()` is
    // called for every click count, so the two click text selection has already
    // been processed in a previous call. Undo that highlight.
    ApplyUndoRedoCommands(undo_redo_model_.Undo());
  }

  std::optional<PdfInkUndoRedoModel::DiscardedDrawCommands> discards =
      undo_redo_model_.StartDraw();
  CHECK(discards.has_value());
  ApplyUndoRedoDiscards(discards.value());

  // Notifying the client will update the text selection.
  client_->OnTextOrLinkAreaClick(position, click_count);

  if (is_double_click || is_triple_click) {
    return FinishTextHighlight(position, /*is_multi_click=*/true, tool_type);
  }

  return true;
}

bool PdfInkModule::ContinueTextHighlight(const gfx::PointF& position) {
  CHECK(is_text_highlighting());
  auto& state = text_highlight_state();
  if (state.finished_multi_click) {
    // This text highlight has already processed multi-click text selection, so
    // do not extend the selection.
    return true;
  }

  if (state.input_last_event.position == position) {
    // Position did not change, so do nothing.
    return true;
  }

  client_->ExtendSelectionByPoint(position);
  state.highlight_strokes = GetTextSelectionAsStrokes();
  state.input_last_event.position = position;
  return true;
}

bool PdfInkModule::FinishTextHighlight(const gfx::PointF& position,
                                       bool is_multi_click,
                                       ink::StrokeInput::ToolType tool_type) {
  CHECK(is_text_highlighting());

  // Process `position` as though it was the last point of movement first, in
  // case a move event was missed.
  ContinueTextHighlight(position);

  auto& state = text_highlight_state();
  if (!state.finished_multi_click) {
    auto& highlight_strokes = state.highlight_strokes;
    highlight_strokes = GetTextSelectionAsStrokes();
    for (const auto& [page_index, strokes] : highlight_strokes) {
      for (const auto& stroke : strokes) {
        InkStrokeId id = stroke_id_generator_.GetIdAndAdvance();
        client_->StrokeAdded(page_index, id, stroke);
        strokes_[page_index].push_back(
            FinishedStrokeState(std::move(stroke), id));
        bool undo_redo_success = undo_redo_model_.Draw(id);
        CHECK(undo_redo_success);
      }

      GenerateAndSendInkThumbnailInternal(page_index);
    }

    const bool modified = !highlight_strokes.empty();
    if (modified) {
      if (state.initiated_by_keyboard) {
        ReportKeyboardTextHighlight(highlighter_brush_.ink_brush());
      } else if (!text_selection_click_timer_.IsRunning()) {
        ReportTextHighlight(highlighter_brush_.ink_brush(), tool_type);
      }

      // Invalidation is already handled by the client during text selection.
    }

    bool undo_redo_success = undo_redo_model_.FinishDraw();
    CHECK(undo_redo_success);

    client_->ClearSelection();

    // Only call StrokeFinished() in this block, where
    // `!state.finished_multi_click` is false.
    client_->StrokeFinished(modified);
  }

  if (is_multi_click) {
    // Stay in text highlight state to handle any additional events.
    state.finished_multi_click = true;

    // Clear the current state's highlight strokes to avoid drawing in-progress
    // strokes on top of the strokes added.
    state.highlight_strokes.clear();
    return true;
  }

  // Reset state back to a drawing highlighter brush. `position` may reference a
  // field in `TextHighlightState`, which would be destroyed after resetting, so
  // check for selectable text or link area first.
  const bool is_text_or_link_area =
      client_->IsSelectableTextOrLinkArea(position);
  current_tool_state_.emplace<DrawingStrokeState>();
  drawing_stroke_state().brush_type = PdfInkBrush::Type::kHighlighter;

  if (!is_text_or_link_area) {
    MaybeSetCursor();
  }
  return true;
}

std::optional<ink::Stroke> PdfInkModule::GetHighlightStrokeFromSelectionRect(
    const gfx::RectF& selection_rect) {
  CHECK(is_text_highlighting());

  TextSelectionHighlightStrokeData stroke_data =
      GetTextSelectionHighlightStrokeData(selection_rect);

  ink::StrokeInputBatch batch;
  ink::StrokeInput input = CreateInkStrokeInput(
      ink::StrokeInput::ToolType::kMouse, stroke_data.first_point,
      /*elapsed_time=*/base::TimeDelta());
  if (!batch.Append(input).ok()) {
    return std::nullopt;
  }

  // Skip the second input point if it matches the first input point.
  if (stroke_data.first_point != stroke_data.second_point) {
    input = CreateInkStrokeInput(ink::StrokeInput::ToolType::kMouse,
                                 stroke_data.second_point,
                                 /*elapsed_time=*/base::TimeDelta());
    if (!batch.Append(input).ok()) {
      return std::nullopt;
    }
  }

  // Make a copy of the ink brush to avoid modifying the drawing highlighter.
  ink::Brush ink_brush = highlighter_brush_.ink_brush();
  if (!ink_brush.SetSize(stroke_data.brush_size).ok()) {
    return std::nullopt;
  }
  return ink::Stroke(ink_brush, batch);
}

PdfInkModule::TextSelectionHighlightStrokeData
PdfInkModule::GetTextSelectionHighlightStrokeData(
    const gfx::RectF& selection_rect) {
  // The stroke should be drawn along the largest dimension, so have the brush
  // size equal the smallest dimension.
  float brush_size = std::min(selection_rect.width(), selection_rect.height());
  bool is_vertical_stroke = brush_size == selection_rect.width();

  // The first input point will always either be the top center of the text
  // characters or the left center of the text characters, depending on whether
  // `selection_rect` is longer vertically. The second input point will be on
  // the opposite end of the rect.
  gfx::PointF start;
  gfx::PointF end;
  if (is_vertical_stroke) {
    start = selection_rect.top_center();
    end = selection_rect.bottom_center();
  } else {
    start = selection_rect.left_center();
    end = selection_rect.right_center();
  }

  // These points need to be offset to account for brush size. Depending on the
  // direction of the stroke, the points will need to be offset in either the x
  // or y axis. Strokes will always be drawn along the largest dimension of the
  // rectangle.
  gfx::Vector2dF offset(brush_size / 2, 0);
  if (is_vertical_stroke) {
    offset.Transpose();
  }
  start += offset;
  end -= offset;
  return TextSelectionHighlightStrokeData{
      .first_point = start, .second_point = end, .brush_size = brush_size};
}

std::map<int, std::vector<ink::Stroke>>
PdfInkModule::GetTextSelectionAsStrokes() {
  std::map<int, std::vector<ink::Stroke>> result;
  for (const auto& [page_index, selection_rects] :
       client_->GetSelectionRectMap()) {
    auto& page_result = result[page_index];
    page_result.reserve(selection_rects.size());
    const gfx::Transform transform =
        client_->GetCanonicalToPdfTransform(page_index).GetCheckedInverse();
    for (const auto& selection_rect : selection_rects) {
      std::optional<ink::Stroke> stroke = GetHighlightStrokeFromSelectionRect(
          transform.MapRect(selection_rect.AsGfxRectF()));
      if (stroke.has_value()) {
        page_result.push_back(stroke.value());
      }
    }
  }
  return result;
}

void PdfInkModule::StartTextSelectionMultiClickTimer(
    ink::StrokeInput::ToolType tool_type) {
  text_selection_click_timer_.Start(
      FROM_HERE, base::Milliseconds(ui::kDoubleClickTimeMs),
      base::BindOnce(&ReportTextHighlight, highlighter_brush_.ink_brush(),
                     tool_type));
}

void PdfInkModule::StopTextSelectionMultiClickTimer() {
  text_selection_click_timer_.Stop();
}

void PdfInkModule::MaybeRecordPenInput(ink::StrokeInput::ToolType tool_type) {
  if (tool_type == ink::StrokeInput::ToolType::kStylus) {
    using_stylus_instead_of_touch_ = true;
  }
}

bool PdfInkModule::ShouldIgnoreTouchInput(
    ink::StrokeInput::ToolType tool_type) {
  return using_stylus_instead_of_touch_ &&
         tool_type == ink::StrokeInput::ToolType::kTouch;
}

void PdfInkModule::HandleAnnotationRedoMessage(
    const base::Value::Dict& message) {
  ApplyUndoRedoCommands(undo_redo_model_.Redo());
}

void PdfInkModule::HandleAnnotationUndoMessage(
    const base::Value::Dict& message) {
  ApplyUndoRedoCommands(undo_redo_model_.Undo());
}

void PdfInkModule::HandleGetAllTextAnnotationsMessage(
    const base::Value::Dict& message) {
  // TODO(crbug.com/408926609): Fill in this method. For now, just return an
  // empty set of annotations.
  client_->PostMessage(
      PrepareReplyMessage(message).Set("annotations", base::Value::List()));
}

void PdfInkModule::HandleGetAnnotationBrushMessage(
    const base::Value::Dict& message) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  base::Value::Dict reply = PrepareReplyMessage(message);

  // Get the brush type from `message` or the current brush type if not
  // provided.
  const std::string* brush_type_message = message.FindString("brushType");
  std::string brush_type_string;
  if (brush_type_message) {
    brush_type_string = *brush_type_message;
  } else {
    brush_type_string =
        is_drawing_stroke()
            ? PdfInkBrush::TypeToString(drawing_stroke_state().brush_type)
            : "eraser";
  }

  base::Value::Dict data;
  data.Set("type", brush_type_string);

  if (brush_type_string == "eraser") {
    reply.Set("data", std::move(data));
    client_->PostMessage(std::move(reply));
    return;
  }

  std::optional<PdfInkBrush::Type> brush_type =
      PdfInkBrush::StringToType(brush_type_string);
  CHECK(brush_type.has_value());

  const ink::Brush& ink_brush = GetBrush(brush_type.value()).ink_brush();
  data.Set("size", ink_brush.GetSize());

  SkColor color = GetSkColorFromInkBrush(ink_brush);
  data.Set("color", base::Value::Dict()
                        .Set("r", static_cast<int>(SkColorGetR(color)))
                        .Set("g", static_cast<int>(SkColorGetG(color)))
                        .Set("b", static_cast<int>(SkColorGetB(color))));

  reply.Set("data", std::move(data));
  client_->PostMessage(std::move(reply));
}

void PdfInkModule::HandleSetAnnotationBrushMessage(
    const base::Value::Dict& message) {
  CHECK_EQ(InkAnnotationMode::kDraw, mode_);

  const base::Value::Dict* data = message.FindDict("data");
  CHECK(data);

  const std::string& brush_type_string = *data->FindString("type");
  if (brush_type_string == "eraser") {
    // TODO(crbug.com/342445982): Handle tool changes during text highlighting.
    if (is_drawing_stroke()) {
      DrawingStrokeState& state = drawing_stroke_state();
      if (state.start_time.has_value()) {
        // PdfInkModule is currently drawing a stroke.  Finish that before
        // transitioning, using the last known input.
        CHECK(state.input_last_event.has_value());
        const EventDetails& input_last_event = state.input_last_event.value();
        FinishStroke(input_last_event.position, input_last_event.timestamp,
                     input_last_event.tool_type, /*properties=*/nullptr);
      }

      current_tool_state_.emplace<EraserState>();
    } else {
      // Do not adjust `current_tool_state_` if an erase stroke is already
      // in-progress.  Changes to the tool state will only apply to subsequent
      // strokes.
      if (!erasing_stroke_state().erasing) {
        current_tool_state_.emplace<EraserState>();
      }
    }

    MaybeSetCursor();
    return;
  }

  float size = base::checked_cast<float>(data->FindDouble("size").value());
  CHECK(PdfInkBrush::IsToolSizeInRange(size));

  if (is_erasing_stroke()) {
    EraserState& state = erasing_stroke_state();
    if (state.erasing) {
      // An erasing stroke is in-progress.  Finish that off before
      // transitioning, using the last known input.
      CHECK(state.input_last_event_position.has_value());
      FinishEraseStroke(state.input_last_event_position.value(),
                        state.tool_type);
    }
  }

  // All brush types except the eraser should have a color and size.
  // TODO(crbug.com/342445982): Handle tool changes during text highlighting.
  const base::Value::Dict* color = data->FindDict("color");
  CHECK(color);

  int color_r = color->FindInt("r").value();
  int color_g = color->FindInt("g").value();
  int color_b = color->FindInt("b").value();

  CheckColorIsWithinRange(color_r);
  CheckColorIsWithinRange(color_g);
  CheckColorIsWithinRange(color_b);

  std::optional<PdfInkBrush::Type> brush_type =
      PdfInkBrush::StringToType(brush_type_string);
  CHECK(brush_type.has_value());
  pending_drawing_brush_state_ = PendingDrawingBrushState{
      SkColorSetRGB(color_r, color_g, color_b), size, brush_type.value()};

  // Do not adjust current tool state if a drawing stroke is already
  // in-progress.  Changes to the tool state will only apply to subsequent
  // strokes.
  if (is_drawing_stroke() && drawing_stroke_state().start_time.has_value()) {
    return;
  }

  if (MaybeSetDrawingBrush()) {
    MaybeSetCursor();
  }
}

void PdfInkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  const std::string* mode = message.FindString("mode");
  CHECK(mode);
  if (*mode == "off") {
    mode_ = InkAnnotationMode::kOff;
  } else if (*mode == "draw") {
    mode_ = InkAnnotationMode::kDraw;
  } else if (*mode == "text") {
    CHECK(features::kPdfInk2TextAnnotations.Get());
    mode_ = InkAnnotationMode::kText;
  } else {
    NOTREACHED();
  }
  client_->OnAnnotationModeToggled(enabled());
  if (enabled() && !loaded_data_from_pdf_) {
    loaded_data_from_pdf_ = true;
    PdfInkModuleClient::DocumentV2InkPathShapesMap loaded_v2_shapes =
        client_->LoadV2InkPathsFromPdf();
    for (auto& [page_index, page_shape_map] : loaded_v2_shapes) {
      PageV2InkPathShapes& page_shapes = loaded_v2_shapes_[page_index];
      page_shapes.reserve(page_shape_map.size());
      for (auto& [shape_id, shape] : page_shape_map) {
        page_shapes.emplace_back(shape, shape_id);
      }
    }
  }
  MaybeSetCursor();
}

void PdfInkModule::HandleStartTextAnnotationMessage(
    const base::Value::Dict& message) {
  // TODO(crbug.com/409439509): Fill in this method. For now, just create it
  // so the backend doesn't CHECK when it's sent from the frontend.
}

void PdfInkModule::HandleFinishTextAnnotationMessage(
    const base::Value::Dict& message) {
  // TODO(crbug.com/409439509): Fill in this method. For now, just create it
  // so the backend doesn't CHECK when it's sent from the frontend.
}

bool PdfInkModule::IsHighlightingTextAtPosition(
    const gfx::PointF& position) const {
  return features::kPdfInk2TextHighlighting.Get() &&
         drawing_stroke_state().brush_type == PdfInkBrush::Type::kHighlighter &&
         client_->IsSelectableTextOrLinkArea(position);
}

PdfInkBrush& PdfInkModule::GetDrawingBrush() {
  // Use the const PdfInkBrush getter and remove the const qualifier to avoid
  // duplicate getter logic.
  return const_cast<PdfInkBrush&>(
      static_cast<PdfInkModule const&>(*this).GetDrawingBrush());
}

const PdfInkBrush& PdfInkModule::GetDrawingBrush() const {
  CHECK(is_drawing_stroke());
  return GetBrush(drawing_stroke_state().brush_type);
}

const PdfInkBrush& PdfInkModule::GetBrush(PdfInkBrush::Type brush_type) const {
  switch (brush_type) {
    case (PdfInkBrush::Type::kHighlighter):
      return highlighter_brush_;
    case (PdfInkBrush::Type::kPen):
      return pen_brush_;
  }
  NOTREACHED();
}

std::vector<ink::InProgressStroke>
PdfInkModule::CreateInProgressStrokeSegmentsFromInputs() const {
  if (!is_drawing_stroke()) {
    return {};
  }

  const DrawingStrokeState& state = drawing_stroke_state();
  const ink::Brush& brush = GetDrawingBrush().ink_brush();
  CHECK(PdfInkBrush::IsToolSizeInRange(brush.GetSize()));
  std::vector<ink::InProgressStroke> stroke_segments;
  stroke_segments.reserve(state.inputs.size());
  for (size_t segment_number = 0; const auto& segment : state.inputs) {
    ++segment_number;
    if (segment.IsEmpty()) {
      // Only the last segment can possibly be empty, if the stroke left the
      // page but never returned back in.
      CHECK_EQ(segment_number, state.inputs.size());
      break;
    }

    ink::InProgressStroke stroke;
    stroke.Start(brush);
    auto enqueue_results =
        stroke.EnqueueInputs(segment, /*predicted_inputs=*/{});
    CHECK(enqueue_results.ok());
    stroke.FinishInputs();
    auto update_results = stroke.UpdateShape(ink::Duration32());
    CHECK(update_results.ok());
    stroke_segments.push_back(std::move(stroke));
  }
  return stroke_segments;
}

gfx::Transform PdfInkModule::GetEventToCanonicalTransformForPage(
    int page_index) {
  // If the page is visible, then its screen rect must not be empty.
  // GetEventToCanonicalTransform() will check this.
  auto page_contents_rect = client_->GetPageContentsRect(page_index);
  return GetEventToCanonicalTransform(client_->GetOrientation(),
                                      page_contents_rect, client_->GetZoom());
}

gfx::Transform PdfInkModule::GetCanonicalToEventTransformForPage(
    int page_index) {
  return GetEventToCanonicalTransformForPage(page_index).GetCheckedInverse();
}

bool PdfInkModule::RecordStrokePosition(
    const gfx::PointF& position,
    base::TimeTicks timestamp,
    ink::StrokeInput::ToolType tool_type,
    const blink::WebPointerProperties* properties) {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  gfx::PointF canonical_position =
      GetEventToCanonicalTransformForPage(state.page_index).MapPoint(position);
  base::TimeDelta time_diff = timestamp - state.start_time.value();

  auto result = state.inputs.back().Append(CreateInkStrokeInputWithProperties(
      tool_type, canonical_position, time_diff, properties));
  return result.ok();
}

void PdfInkModule::ApplyUndoRedoCommands(
    const PdfInkUndoRedoModel::Commands& commands) {
  switch (PdfInkUndoRedoModel::GetCommandsType(commands)) {
    case PdfInkUndoRedoModel::CommandsType::kNone: {
      return;
    }
    case PdfInkUndoRedoModel::CommandsType::kDraw: {
      ApplyUndoRedoCommandsHelper(
          PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
          /*should_draw=*/true);
      return;
    }
    case PdfInkUndoRedoModel::CommandsType::kErase: {
      ApplyUndoRedoCommandsHelper(
          PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
          /*should_draw=*/false);
      return;
    }
  }
  NOTREACHED();
}

void PdfInkModule::ApplyUndoRedoCommandsHelper(
    std::set<PdfInkUndoRedoModel::IdType> ids,
    bool should_draw) {
  CHECK(!ids.empty());

  std::set<InkStrokeId> stroke_ids;
  std::set<InkModeledShapeId> shape_ids;
  for (PdfInkUndoRedoModel::IdType id : ids) {
    bool inserted;
    if (std::holds_alternative<InkStrokeId>(id)) {
      inserted = stroke_ids.insert(std::get<InkStrokeId>(id)).second;
    } else {
      CHECK(std::holds_alternative<InkModeledShapeId>(id));
      inserted = shape_ids.insert(std::get<InkModeledShapeId>(id)).second;
    }
    CHECK(inserted);
  }

  // Sanity check strokes/shapes exist, if this method is being asked to erase
  // them.
  if (!stroke_ids.empty()) {
    CHECK(!strokes_.empty());
  }
  if (!shape_ids.empty()) {
    CHECK(!loaded_v2_shapes_.empty());
  }

  base::flat_set<int> page_indices_with_ink_thumbnail_updates;
  base::flat_set<int> page_indices_with_pdf_thumbnail_updates;
  for (auto& [page_index, page_ink_strokes] : strokes_) {
    std::vector<InkStrokeId> page_ids;
    page_ids.reserve(page_ink_strokes.size());
    for (const auto& stroke : page_ink_strokes) {
      page_ids.push_back(stroke.id);
    }

    std::vector<InkStrokeId> ids_to_apply_command;
    std::ranges::set_intersection(stroke_ids, page_ids,
                                  std::back_inserter(ids_to_apply_command));
    if (ids_to_apply_command.empty()) {
      continue;
    }

    // `it` is always valid, because all the IDs in `ids_to_apply_command` are
    // in `page_ink_strokes`.
    auto it = page_ink_strokes.begin();
    ink::Envelope invalidate_envelope;
    for (InkStrokeId id : ids_to_apply_command) {
      it = std::ranges::lower_bound(
          it, page_ink_strokes.end(), id, {},
          [](const FinishedStrokeState& state) { return state.id; });
      auto& stroke = *it;
      CHECK_NE(stroke.should_draw, should_draw);
      stroke.should_draw = should_draw;
      client_->UpdateStrokeActive(page_index, id, should_draw);

      invalidate_envelope.Add(stroke.stroke.GetShape().Bounds());

      stroke_ids.erase(id);
    }

    client_->Invalidate(CanonicalInkEnvelopeToInvalidationScreenRect(
        invalidate_envelope, GetCanonicalToEventTransformForPage(page_index)));
    page_indices_with_ink_thumbnail_updates.insert(page_index);

    if (stroke_ids.empty()) {
      break;  // Break out of loop if there is no stroke remaining to apply.
    }
  }

  for (auto& [page_index, page_ink_shapes] : loaded_v2_shapes_) {
    std::vector<InkModeledShapeId> page_ids;
    page_ids.reserve(page_ink_shapes.size());
    for (const auto& shape : page_ink_shapes) {
      page_ids.push_back(shape.id);
    }

    std::vector<InkModeledShapeId> ids_to_apply_command;
    std::ranges::set_intersection(shape_ids, page_ids,
                                  std::back_inserter(ids_to_apply_command));
    if (ids_to_apply_command.empty()) {
      continue;
    }

    // `it` is always valid, because all the IDs in `ids_to_apply_command` are
    // in `page_ink_shapes`.
    auto it = page_ink_shapes.begin();
    ink::Envelope invalidate_envelope;
    for (InkModeledShapeId id : ids_to_apply_command) {
      it = std::ranges::lower_bound(
          it, page_ink_shapes.end(), id, {},
          [](const LoadedV2ShapeState& state) { return state.id; });
      auto& shape_state = *it;
      CHECK_NE(shape_state.should_draw, should_draw);
      shape_state.should_draw = should_draw;
      client_->UpdateShapeActive(page_index, shape_state.id, should_draw);

      invalidate_envelope.Add(shape_state.shape.Bounds());

      shape_ids.erase(id);
    }

    client_->Invalidate(CanonicalInkEnvelopeToInvalidationScreenRect(
        invalidate_envelope, GetCanonicalToEventTransformForPage(page_index)));
    page_indices_with_pdf_thumbnail_updates.insert(page_index);

    if (shape_ids.empty()) {
      break;  // Break out of loop if there is no shape remaining to apply.
    }
  }

  RequestThumbnailUpdates(
      /*ink_updates=*/page_indices_with_ink_thumbnail_updates,
      /*pdf_updates=*/page_indices_with_pdf_thumbnail_updates);
}

void PdfInkModule::ApplyUndoRedoDiscards(
    const PdfInkUndoRedoModel::DiscardedDrawCommands& discards) {
  if (discards.empty()) {
    return;
  }

  // Although `discards` contain the full set of IDs to discard, only the first
  // ID is needed here. This is because the `page_ink_strokes` values in
  // `strokes_` are in sorted order. All elements in `page_ink_strokes` with the
  // first ID or larger IDs can be discarded.
  const InkStrokeId start_id = *discards.begin();
  for (auto& [page_index, page_ink_strokes] : strokes_) {
    // Find the first element in `page_ink_strokes` whose ID >= `start_id`.
    auto start = std::ranges::lower_bound(
        page_ink_strokes, start_id, {},
        [](const FinishedStrokeState& state) { return state.id; });
    auto end = page_ink_strokes.end();
    for (auto it = start; it < end; ++it) {
      client_->DiscardStroke(page_index, it->id);
    }
    page_ink_strokes.erase(start, end);
  }

  // Check the pages with strokes and remove the ones that are now empty.
  // Also find the maximum stroke ID that is in use.
  std::optional<InkStrokeId> max_stroke_id;
  for (auto it = strokes_.begin(); it != strokes_.end();) {
    const auto& page_ink_strokes = it->second;
    if (page_ink_strokes.empty()) {
      it = strokes_.erase(it);
    } else {
      max_stroke_id = std::max(max_stroke_id.value_or(InkStrokeId(0)),
                               page_ink_strokes.back().id);
      ++it;
    }
  }

  // Now that some strokes have been discarded, Let the StrokeIdGenerator know
  // there are IDs available for reuse.
  if (max_stroke_id.has_value()) {
    // Since some stroke(s) got discarded, the maximum stroke ID value cannot be
    // the max integer value. Thus adding 1 will not overflow.
    CHECK_NE(max_stroke_id.value(),
             InkStrokeId(std::numeric_limits<size_t>::max()));
    stroke_id_generator_.ResetIdTo(
        InkStrokeId(max_stroke_id.value().value() + 1));
  } else {
    stroke_id_generator_.ResetIdTo(InkStrokeId(0));
  }
}

bool PdfInkModule::MaybeSetDrawingBrush() {
  if (!pending_drawing_brush_state_.has_value()) {
    return false;
  }

  current_tool_state_.emplace<DrawingStrokeState>();
  drawing_stroke_state().brush_type = pending_drawing_brush_state_->type;

  PdfInkBrush& current_brush = GetDrawingBrush();
  current_brush.SetColor(pending_drawing_brush_state_->color);
  current_brush.SetSize(pending_drawing_brush_state_->size);

  pending_drawing_brush_state_.reset();

  return true;
}

void PdfInkModule::MaybeSetCursor() {
  switch (mode_) {
    case InkAnnotationMode::kOff:
      // Do nothing when disabled. The code outside of PdfInkModule will select
      // a normal mouse cursor and switch to that.
      return;

    case InkAnnotationMode::kDraw: {
      if (features::kPdfInk2TextHighlighting.Get() && is_text_highlighting()) {
        return;
      }

      SkColor color;
      float brush_size;
      if (is_drawing_stroke()) {
        const auto& ink_brush = GetDrawingBrush().ink_brush();
        color = GetSkColorFromInkBrush(ink_brush);
        brush_size = ink_brush.GetSize();
      } else {
        CHECK(is_erasing_stroke());
        color = kEraserColor;
        brush_size = kEraserSize;
      }

      SkBitmap bitmap = GenerateToolCursor(
          color,
          CursorDiameterFromBrushSizeAndZoom(brush_size, client_->GetZoom()));
      gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
      client_->UpdateInkCursor(
          ui::Cursor::NewCustom(std::move(bitmap), std::move(hotspot)));
      return;
    }

    case InkAnnotationMode::kText:
      // TODO(crbug.com/402546153): Update cursor for text annotation, once
      // UX determines if it should always use I-beam.
      client_->UpdateInkCursor(ui::mojom::CursorType::kIBeam);
      return;
  }
  NOTREACHED();
}

void PdfInkModule::MaybeSetCursorOnMouseMove(const gfx::PointF& position) {
  if (!features::kPdfInk2TextHighlighting.Get()) {
    return;
  }

  CHECK(is_drawing_stroke());
  if (drawing_stroke_state().brush_type != PdfInkBrush::Type::kHighlighter ||
      !client_->IsSelectableTextOrLinkArea(position)) {
    if (client_->GetCursor().type() == ui::mojom::CursorType::kIBeam) {
      MaybeSetCursor();
    }
    return;
  }

  if (client_->GetCursor().type() != ui::mojom::CursorType::kIBeam) {
    client_->UpdateInkCursor(ui::mojom::CursorType::kIBeam);
  }
}

PdfInkModule::DrawingStrokeState::DrawingStrokeState() = default;

PdfInkModule::DrawingStrokeState::~DrawingStrokeState() = default;

PdfInkModule::EraserState::EraserState() = default;

PdfInkModule::EraserState::~EraserState() = default;

PdfInkModule::TextHighlightState::TextHighlightState() = default;

PdfInkModule::TextHighlightState::~TextHighlightState() = default;

PdfInkModule::FinishedStrokeState::FinishedStrokeState(ink::Stroke stroke,
                                                       InkStrokeId id)
    : stroke(std::move(stroke)), id(id) {}

PdfInkModule::FinishedStrokeState::FinishedStrokeState(
    PdfInkModule::FinishedStrokeState&&) noexcept = default;

PdfInkModule::FinishedStrokeState& PdfInkModule::FinishedStrokeState::operator=(
    PdfInkModule::FinishedStrokeState&&) noexcept = default;

PdfInkModule::FinishedStrokeState::~FinishedStrokeState() = default;

PdfInkModule::LoadedV2ShapeState::LoadedV2ShapeState(ink::PartitionedMesh shape,
                                                     InkModeledShapeId id)
    : shape(std::move(shape)), id(id) {}

PdfInkModule::LoadedV2ShapeState::LoadedV2ShapeState(
    PdfInkModule::LoadedV2ShapeState&&) noexcept = default;

PdfInkModule::LoadedV2ShapeState& PdfInkModule::LoadedV2ShapeState::operator=(
    PdfInkModule::LoadedV2ShapeState&&) noexcept = default;

PdfInkModule::LoadedV2ShapeState::~LoadedV2ShapeState() = default;

PdfInkModule::StrokeIdGenerator::StrokeIdGenerator() = default;

PdfInkModule::StrokeIdGenerator::~StrokeIdGenerator() = default;

InkStrokeId PdfInkModule::StrokeIdGenerator::GetIdAndAdvance() {
  // Die intentionally if `next_stroke_id_` is about to overflow.
  CHECK_NE(next_stroke_id_.value(), std::numeric_limits<size_t>::max());
  InkStrokeId stroke_id = next_stroke_id_;
  ++next_stroke_id_.value();
  return stroke_id;
}

void PdfInkModule::StrokeIdGenerator::ResetIdTo(InkStrokeId id) {
  next_stroke_id_ = id;
}

}  // namespace chrome_pdf
