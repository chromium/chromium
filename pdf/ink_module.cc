// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

#include <memory>
#include <numbers>
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
#include "pdf/ink/ink_brush_family.h"
#include "pdf/ink/ink_brush_paint.h"
#include "pdf/ink/ink_brush_tip.h"
#include "pdf/ink/ink_in_progress_stroke.h"
#include "pdf/ink/ink_stroke.h"
#include "pdf/ink/ink_stroke_input_batch.h"
#include "pdf/input_utils.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

std::string CreateBrushUri() {
  // TODO(crbug.com/335524380): Use real value here.
  return "ink://ink/texture:test-texture";
}

std::unique_ptr<InkBrush> CreateBrush() {
  // TODO(crbug.com/335524380): Use real values here.
  InkBrushTip tip;
  tip.corner_rounding = 0;
  tip.opacity_multiplier = 1.0f;

  InkBrushPaint::TextureLayer layer;
  layer.color_texture_uri = CreateBrushUri();
  layer.mapping = InkBrushPaint::TextureMapping::kWinding;
  layer.size_unit = InkBrushPaint::TextureSizeUnit::kBrushSize;
  layer.size_x = 3;
  layer.size_y = 5;
  layer.size_jitter_x = 0.1;
  layer.size_jitter_y = 2;
  layer.keyframes = {
      {.progress = 0.1, .rotation_in_radians = std::numbers::pi_v<float> / 4}};
  layer.blend_mode = InkBrushPaint::BlendMode::kSrcIn;

  InkBrushPaint paint;
  paint.texture_layers.push_back(layer);
  auto family = InkBrushFamily::Create(tip, paint, "");
  CHECK(family);
  return InkBrush::Create(std::move(family),
                          /*color=*/SkColorSetRGB(0x18, 0x80, 0x38),
                          /*size=*/1.0f, /*epsilon=*/0.1f);
}

}  // namespace

InkModule::InkModule() {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInk2));
}

InkModule::~InkModule() = default;

void InkModule::Draw(SkCanvas& canvas) {
  auto stroke = InkInProgressStroke::Create();
  // TODO(crbug.com/339682315): This should not fail with the wrapper.
  if (!stroke) {
    return;
  }

  std::unique_ptr<InkBrush> brush = CreateBrush();
  CHECK(brush);
  stroke->Start(*brush);
  auto input_batch = InkStrokeInputBatch::Create(ink_inputs_);
  CHECK(input_batch);
  bool enqueue_results = stroke->EnqueueInputs(input_batch.get(), nullptr);
  CHECK(enqueue_results);
  stroke->FinishInputs();
  bool update_results = stroke->UpdateShape(0);
  CHECK(update_results);

  // TODO(crbug.com/335524380): Draw with InkSkiaRenderer. Add more parameters
  // as needed.
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

bool InkModule::OnMouseDown(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  blink::WebMouseEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  // TODO(crbug.com/335517471): Adjust `point` if needed.
  gfx::PointF point = normalized_event.PositionInWidget();
  CHECK(!ink_start_time_.has_value());
  ink_start_time_ = base::Time::Now();
  ink_inputs_.push_back({
      .position_x = point.x(),
      .position_y = point.y(),
      .elapsed_time_seconds = 0,
  });
  return true;
}

bool InkModule::OnMouseUp(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  if (event.button != blink::WebPointerProperties::Button::kLeft) {
    return false;
  }

  if (!ink_start_time_.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  auto stroke = InkInProgressStroke::Create();
  std::unique_ptr<InkBrush> brush = CreateBrush();
  CHECK(brush);
  stroke->Start(*brush);
  // TODO(crbug.com/335524380): Add `event` to `ink_inputs_`?
  auto input_batch = InkStrokeInputBatch::Create(ink_inputs_);
  CHECK(input_batch);
  bool enqueue_results = stroke->EnqueueInputs(input_batch.get(), nullptr);
  CHECK(enqueue_results);
  stroke->FinishInputs();
  bool update_results = stroke->UpdateShape(0);
  CHECK(update_results);
  ink_strokes_.push_back(stroke->CopyToStroke());

  ink_inputs_.clear();

  ink_start_time_ = std::nullopt;
  return true;
}

bool InkModule::OnMouseMove(const blink::WebMouseEvent& event) {
  CHECK(enabled());

  if (!ink_start_time_.has_value()) {
    // Ignore when not drawing.
    return false;
  }

  // TODO(crbug.com/335517471): Adjust `point` if needed.
  gfx::PointF point = event.PositionInWidget();
  base::TimeDelta time_diff = base::Time::Now() - ink_start_time_.value();
  ink_inputs_.push_back({
      .position_x = point.x(),
      .position_y = point.y(),
      .elapsed_time_seconds = static_cast<float>(time_diff.InSecondsF()),
  });

  // TODO(crbug.com/335517471): Invalidate the appropriate rect here.
  return true;
}

void InkModule::HandleSetAnnotationModeMessage(
    const base::Value::Dict& message) {
  enabled_ = message.FindBool("enable").value();
}

}  // namespace chrome_pdf
