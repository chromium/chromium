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
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/draw_utils/page_boundary_intersect.h"
#include "pdf/input_utils.h"
#include "pdf/message_util.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_cursor.h"
#include "pdf/pdf_ink_module_client.h"
#include "pdf/pdf_ink_transform.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/ink/src/ink/brush/brush.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/intersects.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/ink/src/ink/geometry/rect.h"
#include "third_party/ink/src/ink/rendering/skia/native/skia_renderer.h"
#include "third_party/ink/src/ink/strokes/in_progress_stroke.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

// TODO(crbug.com/377733396): Determine if it possible to differentiate between
// touch and pen. Defaulting to touch for now.
constexpr auto kTouchOrPenToolType = ink::StrokeInput::ToolType::kTouch;

constexpr ink::AffineTransform kIdentityTransform;

PdfInkModule::StrokeInputPoints GetStrokePointsForTesting(  // IN-TEST
    const ink::StrokeInputBatch& input_batch) {
  PdfInkModule::StrokeInputPoints stroke_points;
  stroke_points.reserve(input_batch.Size());
  for (size_t i = 0; i < input_batch.Size(); ++i) {
    ink::StrokeInput stroke_input = input_batch.Get(i);
    stroke_points.emplace_back(stroke_input.position.x,
                               stroke_input.position.y);
  }
  return stroke_points;
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

ink::Rect GetEraserRect(const gfx::PointF& center, int distance_to_center) {
  return ink::Rect::FromTwoPoints(
      {center.x() - distance_to_center, center.y() - distance_to_center},
      {center.x() + distance_to_center, center.y() + distance_to_center});
}

SkRect GetDrawPageClipRect(const gfx::Rect& content_rect,
                           const gfx::Vector2dF& origin_offset) {
  gfx::RectF clip_rect(content_rect);
  clip_rect.Offset(origin_offset);
  return gfx::RectFToSkRect(clip_rect);
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

void PdfInkModule::Draw(SkCanvas& canvas) {
  ink::SkiaRenderer skia_renderer;

  const gfx::Vector2dF origin_offset = client_->GetViewportOriginOffset();
  const PageOrientation rotation = client_->GetOrientation();
  const float zoom = client_->GetZoom();

  auto in_progress_stroke = CreateInProgressStrokeSegmentsFromInputs();
  if (in_progress_stroke.empty()) {
    return;
  }

  DrawingStrokeState& state = drawing_stroke_state();

  const gfx::Rect content_rect = client_->GetPageContentsRect(state.page_index);
  const ink::AffineTransform transform =
      GetInkRenderTransform(origin_offset, rotation, content_rect, zoom);

  SkAutoCanvasRestore save_restore(&canvas, /*doSave=*/true);
  canvas.clipRect(GetDrawPageClipRect(content_rect, origin_offset));
  for (const auto& segment : in_progress_stroke) {
    auto status = skia_renderer.Draw(nullptr, segment, transform, canvas);
    CHECK(status.ok());
  }
}

bool PdfInkModule::DrawThumbnail(SkCanvas& canvas, int page_index) {
  auto it = strokes_.find(page_index);
  if (it == strokes_.end() || it->second.empty()) {
    return false;
  }

  // Since thumbnails are always drawn without any rotation, `transform` only
  // needs to perform scaling.
  const SkImageInfo canvas_info = canvas.imageInfo();
  const gfx::Rect content_rect = client_->GetPageContentsRect(page_index);
  const float ratio =
      client_->GetZoom() *
      std::min(
          static_cast<float>(canvas_info.width()) / content_rect.width(),
          static_cast<float>(canvas_info.height()) / content_rect.height());
  const ink::AffineTransform transform = {ratio, 0, 0, 0, ratio, 0};

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

PdfInkModule::PageInkStrokeIterator PdfInkModule::GetVisibleStrokesIterator() {
  return PageInkStrokeIterator(strokes_);
}

bool PdfInkModule::HandleInputEvent(const blink::WebInputEvent& event) {
  if (!enabled()) {
    return false;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseDown: {
      // TODO(crbug.com/353942909): Send a content focused message for certain
      // non-mouse inputs, too.
      base::Value::Dict message;
      message.Set("type", "contentFocused");
      client_->PostMessage(std::move(message));
      return OnMouseDown(static_cast<const blink::WebMouseEvent&>(event));
    }
    case blink::WebInputEvent::Type::kMouseUp:
      return OnMouseUp(static_cast<const blink::WebMouseEvent&>(event));
    case blink::WebInputEvent::Type::kMouseMove:
      return OnMouseMove(static_cast<const blink::WebMouseEvent&>(event));
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
          {"getAnnotationBrush",
           &PdfInkModule::HandleGetAnnotationBrushMessage},
          {"setAnnotationBrush",
           &PdfInkModule::HandleSetAnnotationBrushMessage},
          {"setAnnotationMode", &PdfInkModule::HandleSetAnnotationModeMessage},
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
  MaybeSetCursor();
}

const PdfInkBrush* PdfInkModule::GetPdfInkBrushForTesting() const {
  return is_drawing_stroke() ? &GetDrawingBrush() : nullptr;
}

std::optional<float> PdfInkModule::GetEraserSizeForTesting() const {
  if (is_erasing_stroke()) {
    return eraser_size_;
  }
  return std::nullopt;
}

PdfInkModule::DocumentStrokeInputPointsMap
PdfInkModule::GetStrokesInputPositionsForTesting() const {
  DocumentStrokeInputPointsMap all_strokes_points;

  for (const auto& [page_index, strokes] : strokes_) {
    for (const auto& stroke : strokes) {
      all_strokes_points[page_index].push_back(
          GetStrokePointsForTesting(stroke.stroke.GetInputs()));  // IN-TEST
    }
  }

  return all_strokes_points;
}

PdfInkModule::DocumentStrokeInputPointsMap
PdfInkModule::GetVisibleStrokesInputPositionsForTesting() const {
  DocumentStrokeInputPointsMap all_strokes_points;

  for (const auto& [page_index, strokes] : strokes_) {
    for (const auto& stroke : strokes) {
      if (!stroke.should_draw) {
        continue;
      }

      all_strokes_points[page_index].push_back(
          GetStrokePointsForTesting(stroke.stroke.GetInputs()));  // IN-TEST
    }
  }

  return all_strokes_points;
}

int PdfInkModule::GetInputOfTypeCountForPageForTesting(
    int page_index,
    ink::StrokeInput::ToolType tool_type) const {
  CHECK_GE(page_index, 0);
  auto it = strokes_.find(page_index);
  if (it == strokes_.end()) {
    return 0;
  }

  int count = 0;
  for (const FinishedStrokeState& stroke_state : it->second) {
    const ink::StrokeInputBatch& input_batch = stroke_state.stroke.GetInputs();
    for (ink::StrokeInput input : input_batch) {
      if (input.tool_type == tool_type) {
        ++count;
      }
    }
  }
  return count;
}

bool PdfInkModule::OnMouseDown(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  gfx::PointF position = normalized_event.PositionInWidget();
  return is_drawing_stroke() ? StartStroke(position, event.TimeStamp(),
                                           ink::StrokeInput::ToolType::kMouse)
                             : StartEraseStroke(position);
}

bool PdfInkModule::OnMouseUp(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  if (event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  gfx::PointF position = event.PositionInWidget();
  return is_drawing_stroke() ? FinishStroke(position, event.TimeStamp(),
                                            ink::StrokeInput::ToolType::kMouse)
                             : FinishEraseStroke(position);
}

bool PdfInkModule::OnMouseMove(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  gfx::PointF position = event.PositionInWidget();
  return is_drawing_stroke()
             ? ContinueStroke(position, event.TimeStamp(),
                              ink::StrokeInput::ToolType::kMouse)
             : ContinueEraseStroke(position);
}

bool PdfInkModule::OnTouchStart(const blink::WebTouchEvent& event) {
  CHECK(enabled());

  if (event.touches_length != 1) {
    return false;
  }

  gfx::PointF position = event.touches[0].PositionInWidget();
  return is_drawing_stroke()
             ? StartStroke(position, event.TimeStamp(), kTouchOrPenToolType)
             : StartEraseStroke(position);
}

bool PdfInkModule::OnTouchEnd(const blink::WebTouchEvent& event) {
  CHECK(enabled());

  if (event.touches_length != 1) {
    return false;
  }

  gfx::PointF position = event.touches[0].PositionInWidget();
  return is_drawing_stroke()
             ? FinishStroke(position, event.TimeStamp(), kTouchOrPenToolType)
             : FinishEraseStroke(position);
}

bool PdfInkModule::OnTouchMove(const blink::WebTouchEvent& event) {
  CHECK(enabled());

  if (event.touches_length != 1) {
    return false;
  }

  gfx::PointF position = event.touches[0].PositionInWidget();
  return is_drawing_stroke()
             ? ContinueStroke(position, event.TimeStamp(), kTouchOrPenToolType)
             : ContinueEraseStroke(position);
}

bool PdfInkModule::StartStroke(const gfx::PointF& position,
                               base::TimeTicks timestamp,
                               ink::StrokeInput::ToolType tool_type) {
  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do not draw when not on a page.
    return false;
  }

  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();

  gfx::PointF page_position =
      ConvertEventPositionToCanonicalPosition(position, page_index);

  CHECK(!state.start_time.has_value());
  state.start_time = timestamp;
  state.page_index = page_index;

  // Start of the first segment of a stroke.
  ink::StrokeInputBatch segment;
  auto result =
      segment.Append(CreateInkStrokeInput(tool_type, page_position,
                                          /*elapsed_time=*/base::TimeDelta()));
  CHECK(result.ok());
  state.inputs.push_back(std::move(segment));

  // Invalidate area around this one point.
  client_->Invalidate(GetDrawingBrush().GetInvalidateArea(position, position));

  std::optional<PdfInkUndoRedoModel::DiscardedDrawCommands> discards =
      undo_redo_model_.StartDraw();
  CHECK(discards.has_value());
  ApplyUndoRedoDiscards(discards.value());

  // Remember this location to support invalidating all of the area between
  // this location and the next position.
  CHECK(!state.input_last_event_position.has_value());
  state.input_last_event_position = position;

  return true;
}

bool PdfInkModule::ContinueStroke(const gfx::PointF& position,
                                  base::TimeTicks timestamp,
                                  ink::StrokeInput::ToolType tool_type) {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.start_time.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  CHECK(state.input_last_event_position.has_value());
  const gfx::PointF last_position = state.input_last_event_position.value();
  if (position == last_position) {
    // Since the position did not change, do nothing.
    return true;
  }

  const int page_index = client_->VisiblePageIndexFromPoint(position);
  const int last_page_index = client_->VisiblePageIndexFromPoint(last_position);
  if (page_index != state.page_index && last_page_index != state.page_index) {
    // If `position` is outside the page, and so was `last_position`, then just
    // update `last_position` and treat the event as handled.
    state.input_last_event_position = position;
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
      RecordStrokePosition(boundary_position, timestamp, tool_type);
      client_->Invalidate(GetDrawingBrush().GetInvalidateArea(
          last_position, boundary_position));
    }

    // Remember `position` for use in the next event and treat event as handled.
    state.input_last_event_position = position;
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
      RecordStrokePosition(boundary_position, timestamp, tool_type);
      invalidation_position = boundary_position;
    }
  }

  RecordStrokePosition(position, timestamp, tool_type);

  // Invalidate area covering a straight line between this position and the
  // previous one.
  client_->Invalidate(
      GetDrawingBrush().GetInvalidateArea(position, invalidation_position));

  // Remember `position` for use in the next event.
  state.input_last_event_position = position;

  return true;
}

bool PdfInkModule::FinishStroke(const gfx::PointF& position,
                                base::TimeTicks timestamp,
                                ink::StrokeInput::ToolType tool_type) {
  // Process `position` as though it was the last point of movement first,
  // before moving on to various bookkeeping tasks.
  if (!ContinueStroke(position, timestamp, tool_type)) {
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
        invalidate_envelope, client_->GetOrientation(),
        client_->GetPageContentsRect(state.page_index), client_->GetZoom()));
  }

  client_->StrokeFinished();
  client_->UpdateThumbnail(state.page_index);

  bool undo_redo_success = undo_redo_model_.FinishDraw();
  CHECK(undo_redo_success);

  // Reset `state` now that the stroke operation is done.
  state.inputs.clear();
  state.start_time = std::nullopt;
  state.page_index = -1;
  state.input_last_event_position.reset();
  return true;
}

bool PdfInkModule::StartEraseStroke(const gfx::PointF& position) {
  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do not erase when not on a page.
    return false;
  }

  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  CHECK(!state.erasing);
  state.erasing = true;

  std::optional<PdfInkUndoRedoModel::DiscardedDrawCommands> discards =
      undo_redo_model_.StartErase();
  CHECK(discards.has_value());
  ApplyUndoRedoDiscards(discards.value());

  if (EraseHelper(position, page_index)) {
    state.page_indices_with_erasures.insert(page_index);
  }
  return true;
}

bool PdfInkModule::ContinueEraseStroke(const gfx::PointF& position) {
  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  if (!state.erasing) {
    return false;
  }

  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do nothing when the eraser tool is in use, but the event position is
    // off-page. Treat the event as handled to be consistent with
    // ContinueStroke(), and so that nothing else attempts to handle this event.
    return true;
  }

  if (EraseHelper(position, page_index)) {
    state.page_indices_with_erasures.insert(page_index);
  }
  return true;
}

bool PdfInkModule::FinishEraseStroke(const gfx::PointF& position) {
  // Process `position` as though it was the last point of movement first,
  // before moving on to various bookkeeping tasks.
  if (!ContinueEraseStroke(position)) {
    return false;
  }

  bool undo_redo_success = undo_redo_model_.FinishErase();
  CHECK(undo_redo_success);

  CHECK(is_erasing_stroke());
  EraserState& state = erasing_stroke_state();
  if (!state.page_indices_with_erasures.empty()) {
    client_->StrokeFinished();
    for (int page_index : state.page_indices_with_erasures) {
      client_->UpdateThumbnail(page_index);
    }
  }

  // Reset `state` now that the erase operation is done.
  state.erasing = false;
  state.page_indices_with_erasures.clear();
  return true;
}

bool PdfInkModule::EraseHelper(const gfx::PointF& position, int page_index) {
  CHECK_GE(page_index, 0);

  const gfx::PointF canonical_position =
      ConvertEventPositionToCanonicalPosition(position, page_index);
  const ink::Rect eraser_rect = GetEraserRect(canonical_position, eraser_size_);
  ink::Envelope invalidate_envelope;

  if (auto stroke_it = strokes_.find(page_index); stroke_it != strokes_.end()) {
    for (auto& stroke : stroke_it->second) {
      if (!stroke.should_draw) {
        // Already erased.
        continue;
      }

      // No transform needed, as `eraser_rect` is already using transformed
      // coordinates from `canonical_position`.
      const ink::ModeledShape& shape = stroke.stroke.GetShape();
      if (!ink::Intersects(eraser_rect, shape, kIdentityTransform)) {
        continue;
      }

      stroke.should_draw = false;
      client_->UpdateStrokeActive(page_index, stroke.id, /*active=*/false);

      invalidate_envelope.Add(shape.Bounds());

      bool undo_redo_success = undo_redo_model_.EraseStroke(stroke.id);
      CHECK(undo_redo_success);
    }
  }

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

      bool undo_redo_success = undo_redo_model_.EraseShape(shape_state.id);
      CHECK(undo_redo_success);
    }
  }

  if (invalidate_envelope.IsEmpty()) {
    return false;
  }

  // If `invalidate_envelope` isn't empty, then something got erased.
  client_->Invalidate(CanonicalInkEnvelopeToInvalidationScreenRect(
      invalidate_envelope, client_->GetOrientation(),
      client_->GetPageContentsRect(page_index), client_->GetZoom()));
  return true;
}

void PdfInkModule::HandleAnnotationRedoMessage(
    const base::Value::Dict& message) {
  ApplyUndoRedoCommands(undo_redo_model_.Redo());
}

void PdfInkModule::HandleAnnotationUndoMessage(
    const base::Value::Dict& message) {
  ApplyUndoRedoCommands(undo_redo_model_.Undo());
}

void PdfInkModule::HandleGetAnnotationBrushMessage(
    const base::Value::Dict& message) {
  CHECK(enabled_);

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
    data.Set("size", eraser_size_);
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
  base::Value::Dict color_reply;
  color_reply.Set("r", static_cast<int>(SkColorGetR(color)));
  color_reply.Set("g", static_cast<int>(SkColorGetG(color)));
  color_reply.Set("b", static_cast<int>(SkColorGetB(color)));
  data.Set("color", std::move(color_reply));

  reply.Set("data", std::move(data));
  client_->PostMessage(std::move(reply));
}

void PdfInkModule::HandleSetAnnotationBrushMessage(
    const base::Value::Dict& message) {
  CHECK(enabled_);

  const base::Value::Dict* data = message.FindDict("data");
  CHECK(data);

  float size = base::checked_cast<float>(data->FindDouble("size").value());
  CHECK(PdfInkBrush::IsToolSizeInRange(size));

  const std::string& brush_type_string = *data->FindString("type");
  if (brush_type_string == "eraser") {
    current_tool_state_.emplace<EraserState>();
    eraser_size_ = size;
    MaybeSetCursor();
    return;
  }

  // All brush types except the eraser should have a color and size.
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
  current_tool_state_.emplace<DrawingStrokeState>();
  drawing_stroke_state().brush_type = brush_type.value();

  PdfInkBrush& current_brush = GetDrawingBrush();
  current_brush.SetColor(SkColorSetRGB(color_r, color_g, color_b));
  current_brush.SetSize(size);

  MaybeSetCursor();
}

void PdfInkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  enabled_ = message.FindBool("enable").value();
  client_->OnAnnotationModeToggled(enabled_);
  if (enabled_ && !loaded_data_from_pdf_) {
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

gfx::PointF PdfInkModule::ConvertEventPositionToCanonicalPosition(
    const gfx::PointF& position,
    int page_index) {
  // If the page is visible at `position`, then its rect must not be empty.
  auto page_contents_rect = client_->GetPageContentsRect(page_index);
  CHECK(!page_contents_rect.IsEmpty());

  return EventPositionToCanonicalPosition(position, client_->GetOrientation(),
                                          page_contents_rect,
                                          client_->GetZoom());
}

void PdfInkModule::RecordStrokePosition(const gfx::PointF& position,
                                        base::TimeTicks timestamp,
                                        ink::StrokeInput::ToolType tool_type) {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  gfx::PointF canonical_position =
      ConvertEventPositionToCanonicalPosition(position, state.page_index);
  base::TimeDelta time_diff = timestamp - state.start_time.value();
  auto result = state.inputs.back().Append(
      CreateInkStrokeInput(tool_type, canonical_position, time_diff));
  CHECK(result.ok());
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
    if (absl::holds_alternative<InkStrokeId>(id)) {
      inserted = stroke_ids.insert(absl::get<InkStrokeId>(id)).second;
    } else {
      CHECK(absl::holds_alternative<InkModeledShapeId>(id));
      inserted = shape_ids.insert(absl::get<InkModeledShapeId>(id)).second;
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

  std::set<int> page_indices_with_thumbnail_updates;
  for (auto& [page_index, page_ink_strokes] : strokes_) {
    std::vector<InkStrokeId> page_ids;
    page_ids.reserve(page_ink_strokes.size());
    for (const auto& stroke : page_ink_strokes) {
      page_ids.push_back(stroke.id);
    }

    std::vector<InkStrokeId> ids_to_apply_command;
    base::ranges::set_intersection(stroke_ids, page_ids,
                                   std::back_inserter(ids_to_apply_command));
    if (ids_to_apply_command.empty()) {
      continue;
    }

    // `it` is always valid, because all the IDs in `ids_to_apply_command` are
    // in `page_ink_strokes`.
    auto it = page_ink_strokes.begin();
    ink::Envelope invalidate_envelope;
    for (InkStrokeId id : ids_to_apply_command) {
      it = base::ranges::lower_bound(
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
        invalidate_envelope, client_->GetOrientation(),
        client_->GetPageContentsRect(page_index), client_->GetZoom()));
    page_indices_with_thumbnail_updates.insert(page_index);

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
    base::ranges::set_intersection(shape_ids, page_ids,
                                   std::back_inserter(ids_to_apply_command));
    if (ids_to_apply_command.empty()) {
      continue;
    }

    // `it` is always valid, because all the IDs in `ids_to_apply_command` are
    // in `page_ink_shapes`.
    auto it = page_ink_shapes.begin();
    ink::Envelope invalidate_envelope;
    for (InkModeledShapeId id : ids_to_apply_command) {
      it = base::ranges::lower_bound(
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
        invalidate_envelope, client_->GetOrientation(),
        client_->GetPageContentsRect(page_index), client_->GetZoom()));
    page_indices_with_thumbnail_updates.insert(page_index);

    if (shape_ids.empty()) {
      break;  // Break out of loop if there is no shape remaining to apply.
    }
  }

  for (int page_index : page_indices_with_thumbnail_updates) {
    client_->UpdateThumbnail(page_index);
  }
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
    auto start = base::ranges::lower_bound(
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

void PdfInkModule::MaybeSetCursor() {
  if (!enabled()) {
    // Do nothing when disabled. The code outside of PdfInkModule will select a
    // normal mouse cursor and switch to that.
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
    constexpr SkColor kEraserColor = SK_ColorWHITE;
    color = kEraserColor;
    brush_size = eraser_size_;
  }

  client_->UpdateInkCursorImage(GenerateToolCursor(
      color,
      CursorDiameterFromBrushSizeAndZoom(brush_size, client_->GetZoom())));
}

PdfInkModule::DrawingStrokeState::DrawingStrokeState() = default;

PdfInkModule::DrawingStrokeState::~DrawingStrokeState() = default;

PdfInkModule::EraserState::EraserState() = default;

PdfInkModule::EraserState::~EraserState() = default;

PdfInkModule::FinishedStrokeState::FinishedStrokeState(ink::Stroke stroke,
                                                       InkStrokeId id)
    : stroke(std::move(stroke)), id(id) {}

PdfInkModule::FinishedStrokeState::FinishedStrokeState(
    PdfInkModule::FinishedStrokeState&&) noexcept = default;

PdfInkModule::FinishedStrokeState& PdfInkModule::FinishedStrokeState::operator=(
    PdfInkModule::FinishedStrokeState&&) noexcept = default;

PdfInkModule::FinishedStrokeState::~FinishedStrokeState() = default;

PdfInkModule::LoadedV2ShapeState::LoadedV2ShapeState(ink::ModeledShape shape,
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

PdfInkModule::PageInkStrokeIterator::PageInkStrokeIterator(
    const PdfInkModule::DocumentStrokesMap& strokes)
    : strokes_(strokes), pages_iterator_(strokes_->cbegin()) {
  // Set up internal iterators for the first visible stroke, if there is one.
  AdvanceToNextPageWithVisibleStrokes();
}

PdfInkModule::PageInkStrokeIterator::~PageInkStrokeIterator() = default;

std::optional<PdfInkModule::PageInkStroke>
PdfInkModule::PageInkStrokeIterator::GetNextStrokeAndAdvance() {
  if (pages_iterator_ == strokes_->cend()) {
    return std::nullopt;
  }

  // `page_strokes_iterator_` is set up when finding the page, and is updated
  // after establishing the stroke to return.  So the return value is based
  // upon the current position of the iterator.  Callers should not get here
  // if the end of the strokes has been reached for the current page.
  CHECK(page_strokes_iterator_ != pages_iterator_->second.cend());
  CHECK(page_strokes_iterator_->should_draw);
  const ink::Stroke& page_stroke = page_strokes_iterator_->stroke;
  int page_index = pages_iterator_->first;
  AdvanceForCurrentPage();

  if (page_strokes_iterator_ == pages_iterator_->second.cend()) {
    // This was the last stroke for the current page, so advancing requires
    // moving on to another page and reinitializing `page_strokes_iterator_`.
    ++pages_iterator_;
    AdvanceToNextPageWithVisibleStrokes();
  }

  return PageInkStroke{page_index, raw_ref<const ink::Stroke>(page_stroke)};
}

void PdfInkModule::PageInkStrokeIterator::
    AdvanceToNextPageWithVisibleStrokes() {
  for (; pages_iterator_ != strokes_->cend(); ++pages_iterator_) {
    // Initialize and scan to the location of the first (if any) visible
    // stroke for this page.
    for (page_strokes_iterator_ = pages_iterator_->second.cbegin();
         page_strokes_iterator_ != pages_iterator_->second.cend();
         ++page_strokes_iterator_) {
      if (page_strokes_iterator_->should_draw) {
        // This page has visible strokes, and `page_strokes_iterator_` has
        // been initialized to the position of the first visible stroke.
        return;
      }
    }
  }

  // No pages with visible strokes found.
}

void PdfInkModule::PageInkStrokeIterator::AdvanceForCurrentPage() {
  CHECK(pages_iterator_ != strokes_->cend());

  // Advance the iterator to next visible stroke in this page (if any) before
  // returning.
  do {
    ++page_strokes_iterator_;
    if (page_strokes_iterator_ == pages_iterator_->second.cend()) {
      break;
    }
  } while (!page_strokes_iterator_->should_draw);
}

}  // namespace chrome_pdf
