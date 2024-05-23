// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

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
#include "pdf/input_utils.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
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

InkModule::InkModule(Client& client)
    : client_(client), pdf_ink_brush_(CreateDefaultBrush()) {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInk2));
}

InkModule::~InkModule() = default;

void InkModule::Draw(SkCanvas& canvas) {
  // TODO(crbug.com/335524380): Draw `ink_strokes_` with InkSkiaRenderer.
  // Add more parameters as needed.

  auto in_progress_stroke = CreateInProgressStrokeFromInputs();
  if (in_progress_stroke) {
    // TODO(crbug.com/335524380): Draw `in_progress_stroke` with
    // InkSkiaRenderer.
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
  return pdf_ink_brush_.get();
}

bool InkModule::OnMouseDown(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  // TODO(crbug.com/335517471): Adjust `position` if needed.
  gfx::PointF position = normalized_event.PositionInWidget();
  return pdf_ink_brush_ ? StartInkStroke(position)
                        : StartEraseInkStroke(position);
}

bool InkModule::OnMouseUp(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  if (event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  return pdf_ink_brush_ ? FinishInkStroke() : FinishEraseInkStroke();
}

bool InkModule::OnMouseMove(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  // TODO(crbug.com/335517471): Adjust `position` if needed.
  gfx::PointF position = event.PositionInWidget();
  return pdf_ink_brush_ ? ContinueInkStroke(position)
                        : ContinueEraseInkStroke(position);
}

bool InkModule::StartInkStroke(const gfx::PointF& position) {
  if (client_->VisiblePageIndexFromPoint(position) < 0) {
    // Do not draw when not on a page.
    return false;
  }

  CHECK(!ink_start_time_.has_value());
  ink_start_time_ = base::Time::Now();
  ink_inputs_.push_back({
      .position_x = position.x(),
      .position_y = position.y(),
      .elapsed_time_seconds = 0,
  });
  return true;
}

bool InkModule::ContinueInkStroke(const gfx::PointF& position) {
  if (!ink_start_time_.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  base::TimeDelta time_diff = base::Time::Now() - ink_start_time_.value();
  ink_inputs_.push_back({
      .position_x = position.x(),
      .position_y = position.y(),
      .elapsed_time_seconds = static_cast<float>(time_diff.InSecondsF()),
  });

  // TODO(crbug.com/335517471): Invalidate the appropriate rect here.
  return true;
}

bool InkModule::FinishInkStroke() {
  if (!ink_start_time_.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  // TODO(crbug.com/335524380): Add this method's caller's `event` to
  // `ink_inputs_` before creating `in_progress_stroke`?
  auto in_progress_stroke = CreateInProgressStrokeFromInputs();
  if (in_progress_stroke) {
    ink_strokes_.push_back(in_progress_stroke->CopyToStroke());
  }

  // Reset input fields.
  ink_inputs_.clear();
  ink_start_time_ = std::nullopt;
  return true;
}

bool InkModule::StartEraseInkStroke(const gfx::PointF& position) {
  // TODO(crbug.com/335524381): Implement.
  return false;
}

bool InkModule::ContinueEraseInkStroke(const gfx::PointF& position) {
  // TODO(crbug.com/335524381): Implement.
  return false;
}

bool InkModule::FinishEraseInkStroke() {
  // TODO(crbug.com/335524381): Implement.
  return false;
}

void InkModule::HandleSetAnnotationBrushMessage(
    const base::Value::Dict& message) {
  CHECK(enabled_);

  const std::string& brush_type_string = *message.FindString("brushType");
  if (brush_type_string == "eraser") {
    pdf_ink_brush_.reset();
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
  pdf_ink_brush_ = std::make_unique<PdfInkBrush>(brush_type.value(), params);
}

void InkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  enabled_ = message.FindBool("enable").value();
}

std::unique_ptr<InkInProgressStroke>
InkModule::CreateInProgressStrokeFromInputs() const {
  if (!pdf_ink_brush_) {
    return nullptr;
  }

  auto stroke = InkInProgressStroke::Create();
  // TODO(crbug.com/339682315): This should not fail with the wrapper.
  if (!stroke) {
    return nullptr;
  }

  stroke->Start(pdf_ink_brush_->GetInkBrush());
  auto input_batch = InkStrokeInputBatch::Create(ink_inputs_);
  CHECK(input_batch);
  bool enqueue_results = stroke->EnqueueInputs(input_batch.get(), nullptr);
  CHECK(enqueue_results);
  stroke->FinishInputs();
  bool update_results = stroke->UpdateShape(0);
  CHECK(update_results);
  return stroke;
}

}  // namespace chrome_pdf
