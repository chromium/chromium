// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_module.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/ink/ink_brush.h"
#include "pdf/ink/ink_in_progress_stroke.h"
#include "pdf/ink/ink_skia_renderer.h"
#include "pdf/ink/ink_stroke.h"
#include "pdf/ink/ink_stroke_input_batch.h"
#include "pdf/ink/ink_stroke_input_batch_view.h"
#include "pdf/input_utils.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_transform.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

PdfInkModule::StrokeInputPoints GetStrokePointsForTesting(  // IN-TEST
    const InkStrokeInputBatchView& input_batch) {
  PdfInkModule::StrokeInputPoints stroke_points;
  stroke_points.reserve(input_batch.Size());
  for (size_t i = 0; i < input_batch.Size(); ++i) {
    InkStrokeInput stroke_input = input_batch.Get(i);
    stroke_points.emplace_back(stroke_input.position.x,
                               stroke_input.position.y);
  }
  return stroke_points;
}

// Default to a black pen brush.
std::unique_ptr<PdfInkBrush> CreateDefaultBrush() {
  const PdfInkBrush::Params kDefaultBrushParams = {SK_ColorBLACK, 1.0f};
  return std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen,
                                       kDefaultBrushParams);
}

// Check if `color` is a valid color value within range.
void CheckColorIsWithinRange(int color) {
  CHECK_GE(color, 0);
  CHECK_LE(color, 255);
}

}  // namespace

PdfInkModule::PdfInkModule(Client& client) : client_(client) {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInk2));
  CHECK(is_drawing_stroke());
  drawing_stroke_state().brush = CreateDefaultBrush();
}

PdfInkModule::~PdfInkModule() = default;

void PdfInkModule::Draw(SkCanvas& canvas) {
  auto skia_renderer = InkSkiaRenderer::Create();

  for (const auto& [page_index, page_strokes] : strokes_) {
    if (!client_->IsPageVisible(page_index)) {
      continue;
    }

    // Use an updated transform based on the page and its position in the
    // viewport.
    InkAffineTransform transform = GetInkRenderTransform(
        client_->GetViewportOriginOffset(), client_->GetOrientation(),
        client_->GetPageContentsRect(page_index), client_->GetZoom());
    if (draw_render_transform_callback_for_testing_) {
      draw_render_transform_callback_for_testing_.Run(transform);
    }

    for (const auto& finished_stroke : page_strokes) {
      if (!finished_stroke.should_draw) {
        continue;
      }

      bool success =
          skia_renderer->Draw(*finished_stroke.stroke, transform, canvas);
      CHECK(success);
    }
  }

  auto in_progress_stroke = CreateInProgressStrokeSegmentsFromInputs();
  if (!in_progress_stroke.empty()) {
    DrawingStrokeState& state = drawing_stroke_state();
    InkAffineTransform transform = GetInkRenderTransform(
        client_->GetViewportOriginOffset(), client_->GetOrientation(),
        client_->GetPageContentsRect(state.page_index), client_->GetZoom());
    if (draw_render_transform_callback_for_testing_) {
      draw_render_transform_callback_for_testing_.Run(transform);
    }

    for (const auto& segment : in_progress_stroke) {
      bool success = skia_renderer->Draw(*segment, transform, canvas);
      CHECK(success);
    }
  }
}

bool PdfInkModule::HandleInputEvent(const blink::WebInputEvent& event) {
  if (!enabled()) {
    return false;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseDown:
      return OnMouseDown(static_cast<const blink::WebMouseEvent&>(event));
    case blink::WebInputEvent::Type::kMouseUp:
      return OnMouseUp(static_cast<const blink::WebMouseEvent&>(event));
    case blink::WebInputEvent::Type::kMouseMove:
      return OnMouseMove(static_cast<const blink::WebMouseEvent&>(event));
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

const PdfInkBrush* PdfInkModule::GetPdfInkBrushForTesting() const {
  return is_drawing_stroke() ? drawing_stroke_state().brush.get() : nullptr;
}

PdfInkModule::DocumentStrokeInputPointsMap
PdfInkModule::GetStrokesInputPositionsForTesting() const {
  DocumentStrokeInputPointsMap all_strokes_points;

  for (const auto& [page_index, strokes] : strokes_) {
    for (const auto& stroke : strokes) {
      all_strokes_points[page_index].push_back(
          GetStrokePointsForTesting(stroke.stroke->GetInputs()));  // IN-TEST
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
          GetStrokePointsForTesting(stroke.stroke->GetInputs()));  // IN-TEST
    }
  }

  return all_strokes_points;
}

void PdfInkModule::SetDrawRenderTransformCallbackForTesting(
    RenderTransformCallback callback) {
  draw_render_transform_callback_for_testing_ = std::move(callback);
}

bool PdfInkModule::OnMouseDown(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  gfx::PointF position = normalized_event.PositionInWidget();
  return is_drawing_stroke() ? StartStroke(position)
                             : StartEraseStroke(position);
}

bool PdfInkModule::OnMouseUp(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  if (event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  return is_drawing_stroke() ? FinishStroke() : FinishEraseStroke();
}

bool PdfInkModule::OnMouseMove(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  gfx::PointF position = event.PositionInWidget();
  return is_drawing_stroke() ? ContinueStroke(position)
                             : ContinueEraseStroke(position);
}

bool PdfInkModule::StartStroke(const gfx::PointF& position) {
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
  state.start_time = base::Time::Now();
  state.page_index = page_index;

  // Start of the first segment of a stroke.
  StrokeInputSegment segment;
  segment.push_back({
      .position = InkPoint{page_position.x(), page_position.y()},
      .elapsed_time_seconds = 0,
  });
  state.inputs.push_back(std::move(segment));

  // Invalidate area around this one point.
  client_->Invalidate(state.brush->GetInvalidateArea(position, position));

  // Remember this location to support invalidating all of the area between
  // this location and the next position.
  CHECK(!state.input_last_event_position.has_value());
  state.input_last_event_position = position;

  return true;
}

bool PdfInkModule::ContinueStroke(const gfx::PointF& position) {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.start_time.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index != state.page_index) {
    // Stroke has left the page.  Do not add this input point.
    if (!state.inputs.back().empty()) {
      // Create a new segment to collect any further points.
      state.inputs.push_back(StrokeInputSegment());

      // Even if the last event position was not on the page boundary, no
      // further points are captured in the stroke from that position to this
      // new out-of-bounds position.  So there is no need to invalidate further
      // from it, just drop it since it is now stale for any new points.
      state.input_last_event_position.reset();
    }

    // Treat event as handled.
    return true;
  }

  CHECK_GE(state.page_index, 0);
  gfx::PointF page_position =
      ConvertEventPositionToCanonicalPosition(position, state.page_index);

  base::TimeDelta time_diff = base::Time::Now() - state.start_time.value();
  state.inputs.back().push_back({
      .position = InkPoint{page_position.x(), page_position.y()},
      .elapsed_time_seconds = static_cast<float>(time_diff.InSecondsF()),
  });

  if (state.inputs.back().size() == 1u) {
    // This is the start of a new segment, so only invalidate around this point.
    CHECK(!state.input_last_event_position.has_value());
    client_->Invalidate(state.brush->GetInvalidateArea(position, position));
  } else {
    // Invalidate area covering a straight line between this position and the
    // previous one.  Update last location to support invalidating from here to
    // the next position.
    CHECK(state.input_last_event_position.has_value());
    client_->Invalidate(state.brush->GetInvalidateArea(
        position, state.input_last_event_position.value()));
  }

  // Update last location to support invalidating from here to
  // the next position.
  state.input_last_event_position = position;

  return true;
}

bool PdfInkModule::FinishStroke() {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.start_time.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  // TODO(crbug.com/335524380): Add this method's caller's `event` to `inputs`
  // before creating `in_progress_stroke_segments`?
  auto in_progress_stroke_segments = CreateInProgressStrokeSegmentsFromInputs();
  if (!in_progress_stroke_segments.empty()) {
    CHECK_GE(state.page_index, 0);
    for (const auto& segment : in_progress_stroke_segments) {
      size_t id = stroke_id_generator_.GetIdAndAdvance();
      strokes_[state.page_index].push_back(
          FinishedStrokeState(segment->CopyToStroke(), id));
    }
  }

  // Reset input fields.
  state.inputs.clear();
  state.start_time = std::nullopt;
  state.page_index = -1;
  state.input_last_event_position.reset();

  client_->StrokeFinished();
  return true;
}

bool PdfInkModule::StartEraseStroke(const gfx::PointF& position) {
  CHECK(is_erasing_stroke());
  // TODO(crbug.com/335524381): Implement.
  // TODO(crbug.com/335517471): Adjust `position` if needed.
  return false;
}

bool PdfInkModule::ContinueEraseStroke(const gfx::PointF& position) {
  CHECK(is_erasing_stroke());
  // TODO(crbug.com/335524381): Implement.
  // TODO(crbug.com/335517471): Adjust `position` if needed.
  return false;
}

bool PdfInkModule::FinishEraseStroke() {
  CHECK(is_erasing_stroke());
  // TODO(crbug.com/335524381): Implement.
  // Call client_->InkStrokeFinished() on success.
  return false;
}

void PdfInkModule::HandleAnnotationRedoMessage(
    const base::Value::Dict& message) {
  CHECK(enabled_);
  // TODO(crbug.com/335521182): Implement redo.
}

void PdfInkModule::HandleAnnotationUndoMessage(
    const base::Value::Dict& message) {
  CHECK(enabled_);
  // TODO(crbug.com/335521182): Implement undo.
}

void PdfInkModule::HandleSetAnnotationBrushMessage(
    const base::Value::Dict& message) {
  CHECK(enabled_);

  const std::string& brush_type_string = *message.FindString("brushType");
  if (brush_type_string == "eraser") {
    current_tool_state_.emplace<EraserState>();
    return;
  }

  // All brush types except the eraser should have a color and size.
  int color_r = message.FindInt("colorR").value();
  int color_g = message.FindInt("colorG").value();
  int color_b = message.FindInt("colorB").value();
  double size = message.FindDouble("size").value();

  CheckColorIsWithinRange(color_r);
  CheckColorIsWithinRange(color_g);
  CheckColorIsWithinRange(color_b);

  PdfInkBrush::Params params;
  params.color = SkColorSetRGB(color_r, color_g, color_b);

  // TODO(crbug.com/341282609): Properly scale the brush size here. The
  // extension uses values from range [0, 1], which will be translated to range
  // [1, 8] for now.
  CHECK_GE(size, 0);
  CHECK_LE(size, 1);

  constexpr float kSizeScaleFactor = 7.0f;
  constexpr float kMinSize = 1.0f;
  params.size = (static_cast<float>(size) * kSizeScaleFactor) + kMinSize;

  std::optional<PdfInkBrush::Type> brush_type =
      PdfInkBrush::StringToType(brush_type_string);
  CHECK(brush_type.has_value());
  current_tool_state_.emplace<DrawingStrokeState>();
  drawing_stroke_state().brush =
      std::make_unique<PdfInkBrush>(brush_type.value(), params);
}

void PdfInkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  enabled_ = message.FindBool("enable").value();
}

std::vector<std::unique_ptr<InkInProgressStroke>>
PdfInkModule::CreateInProgressStrokeSegmentsFromInputs() const {
  if (!is_drawing_stroke()) {
    return {};
  }

  const DrawingStrokeState& state = drawing_stroke_state();
  std::vector<std::unique_ptr<InkInProgressStroke>> stroke_segments;
  stroke_segments.reserve(state.inputs.size());
  for (size_t segment_number = 0; const auto& segment : state.inputs) {
    ++segment_number;
    if (segment.empty()) {
      // Only the last segment can possibly be empty, if the stroke left the
      // page but never returned back in.
      CHECK_EQ(segment_number, state.inputs.size());
      break;
    }

    auto stroke = InkInProgressStroke::Create();
    // TODO(crbug.com/339682315): This should not fail with the wrapper.
    if (!stroke) {
      return {};
    }

    auto input_batch = InkStrokeInputBatch::Create(segment);
    CHECK(input_batch);

    stroke->Start(state.brush->GetInkBrush());
    bool enqueue_results = stroke->EnqueueInputs(input_batch.get(), nullptr);
    CHECK(enqueue_results);
    stroke->FinishInputs();
    bool update_results = stroke->UpdateShape(0);
    CHECK(update_results);
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

PdfInkModule::DrawingStrokeState::DrawingStrokeState() = default;

PdfInkModule::DrawingStrokeState::~DrawingStrokeState() = default;

PdfInkModule::FinishedStrokeState::FinishedStrokeState(
    std::unique_ptr<InkStroke> stroke,
    size_t id)
    : stroke(std::move(stroke)), id(id) {}

PdfInkModule::FinishedStrokeState::FinishedStrokeState(
    PdfInkModule::FinishedStrokeState&&) noexcept = default;

PdfInkModule::FinishedStrokeState& PdfInkModule::FinishedStrokeState::operator=(
    PdfInkModule::FinishedStrokeState&&) noexcept = default;

PdfInkModule::FinishedStrokeState::~FinishedStrokeState() = default;

PdfInkModule::StrokeIdGenerator::StrokeIdGenerator() = default;

PdfInkModule::StrokeIdGenerator::~StrokeIdGenerator() = default;

size_t PdfInkModule::StrokeIdGenerator::GetIdAndAdvance() {
  // Die intentionally if `next_stroke_id_` is about to overflow.
  CHECK_NE(next_stroke_id_, std::numeric_limits<size_t>::max());
  return next_stroke_id_++;
}

}  // namespace chrome_pdf
