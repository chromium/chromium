// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_signature_creation_handler.h"

#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/message_util.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_module_client.h"
#include "third_party/ink/src/ink/geometry/mutable_mesh.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/strokes/in_progress_stroke.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/types/duration.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

namespace {

constexpr SkColor kSignatureBrushColor = SK_ColorBLACK;
constexpr float kSignatureBrushSize = 2.0f;

void AppendSignatureStrokePoint(ink::StrokeInputBatch& batch,
                                const base::DictValue& dict) {
  gfx::PointF position(dict.FindDouble("x").value(),
                       dict.FindDouble("y").value());
  base::TimeDelta elapsed_time =
      base::Milliseconds(dict.FindDouble("elapsedTime").value());
  ink::StrokeInput input = CreateInkStrokeInput(
      ink::StrokeInput::ToolType::kMouse, position, elapsed_time);
  // Ignore duplicate or non-advancing inputs within a coalesced batch so the
  // remaining points can still be enqueued, matching how
  // `ink::InProgressStroke::EnqueueInputs()` skips them across single-point
  // batches.
  batch.Append(input).IgnoreError();
}

// Returns coat 0 mesh vertex outline loops for the in-progress stroke. Each
// loop is flattened into [x0, y0, x1, y1, ...] coordinate pairs to minimize
// IPC serialization overhead during high-frequency pointer movement.
base::ListValue GetOutlinesFromInProgressStroke(
    const ink::InProgressStroke& stroke) {
  CHECK_EQ(stroke.BrushCoatCount(), 1u);
  const auto& outlines = stroke.GetCoatOutlines(0);
  auto outlines_list = base::ListValue::with_capacity(outlines.size());
  const ink::MutableMesh& mesh = stroke.GetMesh(0);
  for (const auto& outline : outlines) {
    auto loop = base::ListValue::with_capacity(outline.size() * 2);
    for (uint32_t vertex_idx : outline) {
      ink::Point pt = mesh.VertexPosition(vertex_idx);
      loop.Append(pt.x);
      loop.Append(pt.y);
    }
    outlines_list.Append(std::move(loop));
  }
  return outlines_list;
}

}  // namespace

PdfInkSignatureCreationHandler::PdfInkSignatureCreationHandler(
    PdfInkModuleClient& client)
    : client_(client) {
  CHECK(features::kPdfInk2Signatures.Get());
}

PdfInkSignatureCreationHandler::~PdfInkSignatureCreationHandler() = default;

bool PdfInkSignatureCreationHandler::OnMessage(const base::DictValue& message) {
  using MessageHandler =
      void (PdfInkSignatureCreationHandler::*)(const base::DictValue&);

  static constexpr auto kMessageHandlers = base::MakeFixedFlatMap<
      std::string_view, MessageHandler>({
      {"continueSignatureStroke",
       &PdfInkSignatureCreationHandler::HandleContinueSignatureStrokeMessage},
      {"finishSignatureStroke",
       &PdfInkSignatureCreationHandler::HandleFinishSignatureStrokeMessage},
      {"startSignatureStroke",
       &PdfInkSignatureCreationHandler::HandleStartSignatureStrokeMessage},
  });

  auto it = kMessageHandlers.find(*message.FindString("type"));
  if (it == kMessageHandlers.end()) {
    return false;
  }

  MessageHandler handler = it->second;
  (this->*handler)(message);
  return true;
}

void PdfInkSignatureCreationHandler::HandleStartSignatureStrokeMessage(
    const base::DictValue& message) {
  const base::DictValue* point = message.FindDict("point");
  CHECK(point);

  PdfInkBrush signature_pen(PdfInkBrush::Type::kPen, kSignatureBrushColor,
                            kSignatureBrushSize);
  in_progress_stroke_.Start(signature_pen.ink_brush());
  ink::StrokeInputBatch batch;
  AppendSignatureStrokePoint(batch, *point);
  CHECK(in_progress_stroke_.EnqueueInputs(batch, /*predicted_inputs=*/{}).ok());
  CHECK(in_progress_stroke_.UpdateShape(ink::Duration32()).ok());

  ReplyWithStrokeOutlines(message);
}

void PdfInkSignatureCreationHandler::HandleContinueSignatureStrokeMessage(
    const base::DictValue& message) {
  CHECK(!in_progress_stroke_.InputsAreFinished());
  const base::ListValue* points = message.FindList("points");
  CHECK(points);
  CHECK(!points->empty());

  ink::StrokeInputBatch batch;
  for (const auto& item : *points) {
    AppendSignatureStrokePoint(batch, item.GetDict());
  }
  CHECK(in_progress_stroke_.EnqueueInputs(batch, /*predicted_inputs=*/{}).ok());
  CHECK(in_progress_stroke_.UpdateShape(ink::Duration32()).ok());

  ReplyWithStrokeOutlines(message);
}

void PdfInkSignatureCreationHandler::HandleFinishSignatureStrokeMessage(
    const base::DictValue& message) {
  CHECK(!in_progress_stroke_.InputsAreFinished());
  if (const base::DictValue* point = message.FindDict("point")) {
    ink::StrokeInputBatch batch;
    AppendSignatureStrokePoint(batch, *point);
    CHECK(
        in_progress_stroke_.EnqueueInputs(batch, /*predicted_inputs=*/{}).ok());
  }

  in_progress_stroke_.FinishInputs();
  CHECK(in_progress_stroke_.UpdateShape(ink::Duration32()).ok());

  ReplyWithStrokeOutlines(message);
  in_progress_stroke_.Clear();
}

void PdfInkSignatureCreationHandler::ReplyWithStrokeOutlines(
    const base::DictValue& message) {
  client_->PostMessage(PrepareReplyMessage(message).Set(
      "outlines", GetOutlinesFromInProgressStroke(in_progress_stroke_)));
}

}  // namespace chrome_pdf
