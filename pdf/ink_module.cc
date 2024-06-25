// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

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

InkModule::InkModule(Client& client) : client_(client) {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInk2));
  CHECK(is_drawing_stroke());
  drawing_stroke_state().ink_brush = CreateDefaultBrush();
}

InkModule::~InkModule() = default;

void InkModule::Draw(SkCanvas& canvas) {
  for (const auto& [page_index, page_ink_strokes] : ink_strokes_) {
    if (!client_->IsPageVisible(page_index)) {
      continue;
    }

    // Use an updated transform based on the page and its position in the
    // viewport.
    // TODO(crbug.com/335524380): Draw `ink_strokes_` with InkSkiaRenderer
    // using the canonical-to-screen rendering transform.
    InkAffineTransform transform = GetInkRenderTransform(
        client_->GetViewportOriginOffset(), client_->GetOrientation(),
        client_->GetPageContentsRect(page_index), client_->GetZoom());
    if (draw_render_transform_callback_for_testing_) {
      draw_render_transform_callback_for_testing_.Run(transform);
    }
  }

  auto in_progress_stroke = CreateInProgressStrokeSegmentsFromInputs();
  if (!in_progress_stroke.empty()) {
    DrawingStrokeState& state = drawing_stroke_state();
    // TODO(crbug.com/335524380): Draw `in_progress_stroke` with InkSkiaRenderer
    // using the canonical-to-screen rendering transform.
    InkAffineTransform transform = GetInkRenderTransform(
        client_->GetViewportOriginOffset(), client_->GetOrientation(),
        client_->GetPageContentsRect(state.ink_page_index), client_->GetZoom());
    if (draw_render_transform_callback_for_testing_) {
      draw_render_transform_callback_for_testing_.Run(transform);
    }
  }
}

bool InkModule::HandleInputEvent(const blink::WebInputEvent& event) {
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

bool InkModule::OnMessage(const base::Value::Dict& message) {
  using MessageHandler = void (InkModule::*)(const base::Value::Dict&);

  static constexpr auto kMessageHandlers =
      base::MakeFixedFlatMap<std::string_view, MessageHandler>({
          {"annotationRedo", &InkModule::HandleAnnotationUndoMessage},
          {"annotationUndo", &InkModule::HandleAnnotationRedoMessage},
          {"setAnnotationBrush", &InkModule::HandleSetAnnotationBrushMessage},
          {"setAnnotationMode", &InkModule::HandleSetAnnotationModeMessage},
      });

  auto it = kMessageHandlers.find(*message.FindString("type"));
  if (it == kMessageHandlers.end()) {
    return false;
  }

  MessageHandler handler = it->second;
  (this->*handler)(message);
  return true;
}

const PdfInkBrush* InkModule::GetPdfInkBrushForTesting() const {
  return is_drawing_stroke() ? drawing_stroke_state().ink_brush.get() : nullptr;
}

InkModule::DocumentInkStrokeInputPointsMap
InkModule::GetInkStrokesInputPositionsForTesting() const {
  DocumentInkStrokeInputPointsMap all_strokes_points;

  for (const auto& [page_index, strokes] : ink_strokes_) {
    for (const auto& stroke : strokes) {
      const InkStrokeInputBatchView& input_batch = stroke.stroke->GetInputs();
      InkModule::InkStrokeInputPoints stroke_points;
      stroke_points.reserve(input_batch.Size());
      for (size_t i = 0; i < input_batch.Size(); ++i) {
        InkStrokeInput stroke_input = input_batch.Get(i);
        stroke_points.emplace_back(stroke_input.position_x,
                                   stroke_input.position_y);
      }
      all_strokes_points[page_index].push_back(std::move(stroke_points));
    }
  }

  return all_strokes_points;
}

void InkModule::SetDrawRenderTransformCallbackForTesting(
    RenderTransformCallback callback) {
  draw_render_transform_callback_for_testing_ = std::move(callback);
}

bool InkModule::OnMouseDown(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  gfx::PointF position = normalized_event.PositionInWidget();
  return is_drawing_stroke() ? StartInkStroke(position)
                             : StartEraseInkStroke(position);
}

bool InkModule::OnMouseUp(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  if (event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  return is_drawing_stroke() ? FinishInkStroke() : FinishEraseInkStroke();
}

bool InkModule::OnMouseMove(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  gfx::PointF position = event.PositionInWidget();
  return is_drawing_stroke() ? ContinueInkStroke(position)
                             : ContinueEraseInkStroke(position);
}

bool InkModule::StartInkStroke(const gfx::PointF& position) {
  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index < 0) {
    // Do not draw when not on a page.
    return false;
  }

  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();

  gfx::PointF page_position =
      ConvertEventPositionToCanonicalPosition(position, page_index);

  CHECK(!state.ink_start_time.has_value());
  state.ink_start_time = base::Time::Now();
  state.ink_page_index = page_index;

  // Start of the first segment of a stroke.
  StrokeInputSegment segment;
  segment.push_back({
      .position_x = page_position.x(),
      .position_y = page_position.y(),
      .elapsed_time_seconds = 0,
  });
  state.ink_inputs.push_back(std::move(segment));

  // Invalidate area around this one point.
  client_->Invalidate(state.ink_brush->GetInvalidateArea(position, position));

  // Remember this location to support invalidating all of the area between
  // this location and the next position.
  CHECK(!state.ink_input_last_event_position.has_value());
  state.ink_input_last_event_position = position;

  return true;
}

bool InkModule::ContinueInkStroke(const gfx::PointF& position) {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.ink_start_time.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  int page_index = client_->VisiblePageIndexFromPoint(position);
  if (page_index != state.ink_page_index) {
    // Stroke has left the page.  Do not add this input point.
    if (!state.ink_inputs.back().empty()) {
      // Create a new segment to collect any further points.
      state.ink_inputs.push_back(StrokeInputSegment());

      // Even if the last event position was not on the page boundary, no
      // further points are captured in the stroke from that position to this
      // new out-of-bounds position.  So there is no need to invalidate further
      // from it, just drop it since it is now stale for any new points.
      state.ink_input_last_event_position.reset();
    }

    // Treat event as handled.
    return true;
  }

  CHECK_GE(state.ink_page_index, 0);
  gfx::PointF page_position =
      ConvertEventPositionToCanonicalPosition(position, state.ink_page_index);

  base::TimeDelta time_diff = base::Time::Now() - state.ink_start_time.value();
  state.ink_inputs.back().push_back({
      .position_x = page_position.x(),
      .position_y = page_position.y(),
      .elapsed_time_seconds = static_cast<float>(time_diff.InSecondsF()),
  });

  if (state.ink_inputs.back().size() == 1u) {
    // This is the start of a new segment, so only invalidate around this point.
    CHECK(!state.ink_input_last_event_position.has_value());
    client_->Invalidate(state.ink_brush->GetInvalidateArea(position, position));
  } else {
    // Invalidate area covering a straight line between this position and the
    // previous one.  Update last location to support invalidating from here to
    // the next position.
    CHECK(state.ink_input_last_event_position.has_value());
    client_->Invalidate(state.ink_brush->GetInvalidateArea(
        position, state.ink_input_last_event_position.value()));
  }

  // Update last location to support invalidating from here to
  // the next position.
  state.ink_input_last_event_position = position;

  return true;
}

bool InkModule::FinishInkStroke() {
  CHECK(is_drawing_stroke());
  DrawingStrokeState& state = drawing_stroke_state();
  if (!state.ink_start_time.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  // TODO(crbug.com/335524380): Add this method's caller's `event` to
  // `ink_inputs` before creating `in_progress_stroke_segments`?
  auto in_progress_stroke_segments = CreateInProgressStrokeSegmentsFromInputs();
  if (!in_progress_stroke_segments.empty()) {
    CHECK_GE(state.ink_page_index, 0);
    for (const auto& segment : in_progress_stroke_segments) {
      size_t id = stroke_id_generator_.GetIdAndAdvance();
      ink_strokes_[state.ink_page_index].push_back(
          FinishedStrokeState(segment->CopyToStroke(), id));
    }
  }

  // Reset input fields.
  state.ink_inputs.clear();
  state.ink_start_time = std::nullopt;
  state.ink_page_index = -1;
  state.ink_input_last_event_position.reset();

  client_->InkStrokeFinished();
  return true;
}

bool InkModule::StartEraseInkStroke(const gfx::PointF& position) {
  CHECK(is_erasing_stroke());
  // TODO(crbug.com/335524381): Implement.
  // TODO(crbug.com/335517471): Adjust `position` if needed.
  return false;
}

bool InkModule::ContinueEraseInkStroke(const gfx::PointF& position) {
  CHECK(is_erasing_stroke());
  // TODO(crbug.com/335524381): Implement.
  // TODO(crbug.com/335517471): Adjust `position` if needed.
  return false;
}

bool InkModule::FinishEraseInkStroke() {
  CHECK(is_erasing_stroke());
  // TODO(crbug.com/335524381): Implement.
  // Call client_->InkStrokeFinished() on success.
  return false;
}

void InkModule::HandleAnnotationRedoMessage(const base::Value::Dict& message) {
  CHECK(enabled_);
  // TODO(crbug.com/335521182): Implement redo.
}

void InkModule::HandleAnnotationUndoMessage(const base::Value::Dict& message) {
  CHECK(enabled_);
  // TODO(crbug.com/335521182): Implement undo.
}

void InkModule::HandleSetAnnotationBrushMessage(
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
  drawing_stroke_state().ink_brush =
      std::make_unique<PdfInkBrush>(brush_type.value(), params);
}

void InkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  enabled_ = message.FindBool("enable").value();
}

std::vector<std::unique_ptr<InkInProgressStroke>>
InkModule::CreateInProgressStrokeSegmentsFromInputs() const {
  if (!is_drawing_stroke()) {
    return {};
  }

  const DrawingStrokeState& state = drawing_stroke_state();
  std::vector<std::unique_ptr<InkInProgressStroke>> stroke_segments;
  stroke_segments.reserve(state.ink_inputs.size());
  for (size_t segment_number = 0; const auto& segment : state.ink_inputs) {
    ++segment_number;
    if (segment.empty()) {
      // Only the last segment can possibly be empty, if the stroke left the
      // page but never returned back in.
      CHECK_EQ(segment_number, state.ink_inputs.size());
      break;
    }

    auto stroke = InkInProgressStroke::Create();
    // TODO(crbug.com/339682315): This should not fail with the wrapper.
    if (!stroke) {
      return {};
    }

    auto input_batch = InkStrokeInputBatch::Create(segment);
    CHECK(input_batch);

    stroke->Start(state.ink_brush->GetInkBrush());
    bool enqueue_results = stroke->EnqueueInputs(input_batch.get(), nullptr);
    CHECK(enqueue_results);
    stroke->FinishInputs();
    bool update_results = stroke->UpdateShape(0);
    CHECK(update_results);
    stroke_segments.push_back(std::move(stroke));
  }
  return stroke_segments;
}

gfx::PointF InkModule::ConvertEventPositionToCanonicalPosition(
    const gfx::PointF& position,
    int page_index) {
  // If the page is visible at `position`, then its rect must not be empty.
  auto page_contents_rect = client_->GetPageContentsRect(page_index);
  CHECK(!page_contents_rect.IsEmpty());

  return EventPositionToCanonicalPosition(position, client_->GetOrientation(),
                                          page_contents_rect,
                                          client_->GetZoom());
}

InkModule::DrawingStrokeState::DrawingStrokeState() = default;

InkModule::DrawingStrokeState::~DrawingStrokeState() = default;

InkModule::FinishedStrokeState::FinishedStrokeState(
    std::unique_ptr<InkStroke> stroke,
    size_t id)
    : stroke(std::move(stroke)), id(id) {}

InkModule::FinishedStrokeState::FinishedStrokeState(
    InkModule::FinishedStrokeState&&) noexcept = default;

InkModule::FinishedStrokeState& InkModule::FinishedStrokeState::operator=(
    InkModule::FinishedStrokeState&&) noexcept = default;

InkModule::FinishedStrokeState::~FinishedStrokeState() = default;

InkModule::StrokeIdGenerator::StrokeIdGenerator() = default;

InkModule::StrokeIdGenerator::~StrokeIdGenerator() = default;

size_t InkModule::StrokeIdGenerator::GetIdAndAdvance() {
  // Die intentionally if `next_stroke_id_` is about to overflow.
  CHECK_NE(next_stroke_id_, std::numeric_limits<size_t>::max());
  return next_stroke_id_++;
}

}  // namespace chrome_pdf
