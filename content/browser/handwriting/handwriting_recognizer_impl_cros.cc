// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognizer_impl_cros.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

// The callback for `mojom::MachineLearningService::LoadHandwritingModel`
// (CrOS).
void OnModelBinding(
    mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer> remote,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback,
    chromeos::machine_learning::mojom::LoadHandwritingModelResult result) {
  if (result ==
      chromeos::machine_learning::mojom::LoadHandwritingModelResult::OK) {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kOk,
        std::move(remote));
  } else {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kError,
        mojo::NullRemote());
  }
}

// The callback for `mojom::HandwritingRecognizer::Recognize` (CrOS).
void OnRecognitionResult(
    std::vector<handwriting::mojom::HandwritingStrokePtr> strokes,
    CrOSHandwritingRecognizerImpl::GetPredictionCallback callback,
    chromeos::machine_learning::mojom::HandwritingRecognizerResultPtr
        result_from_mlservice) {
  if (result_from_mlservice->status !=
      chromeos::machine_learning::mojom::HandwritingRecognizerResult::Status::
          OK) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::vector<handwriting::mojom::HandwritingPredictionPtr> result_to_blink;
  for (const auto& candidate_ml : result_from_mlservice->candidates) {
    auto prediction_blink = handwriting::mojom::HandwritingPrediction::New();
    prediction_blink->text = candidate_ml->text;

    // TODO(https://crbug.com/1181122): We should move the segmentation
    // conversion code to the backend.
    // For gesture model, there is no segmentation so candidate_ml->segmentation
    // is null.
    if (candidate_ml->segmentation.is_null()) {
      result_to_blink.push_back(std::move(prediction_blink));
      continue;
    }
    // TODO(honglinyu): The index calculation may be wrong for unicode
    // strings. But this should be OK for now because we currently only
    // support English.
    // TODO(honglinyu): Consider using `mojo::StructTraits` for the
    // conversions.
    int idx_in_text = 0;
    for (const auto& seg_ml : candidate_ml->segmentation->segments) {
      auto seg_blink = handwriting::mojom::HandwritingSegment::New();
      seg_blink->grapheme = seg_ml->sublabel;
      seg_blink->begin_index = idx_in_text;
      idx_in_text += seg_ml->sublabel.length();
      seg_blink->end_index = idx_in_text;
      for (const auto& ink_range : seg_ml->ink_ranges) {
        // `ink_range->end_stroke` has to be smaller than `strokes.size()`.
        // This check is important because otherwise, the code
        // `strokes[stroke_idx]` below may crash.
        if (ink_range->end_stroke >= strokes.size()) {
          // `base::nullopt` is a signal of error.
          std::move(callback).Run(base::nullopt);
          return;
        }
        for (unsigned int stroke_idx = ink_range->start_stroke;
             stroke_idx <= ink_range->end_stroke; ++stroke_idx) {
          auto draw_seg = handwriting::mojom::HandwritingDrawingSegment::New();
          draw_seg->stroke_index = stroke_idx;
          // The way CrOS's backend designates the strokes belonging to a
          // grapheme is different from that of the Javascript API and the
          // handwriting.mojom file in the renderer. It covers a range of
          // strokes. And the first and last strokes in the range may not
          // fully belong to the grapheme. Specifically, the meaning of
          // members of CrOS's backend's `HandwritingRecognizerInkRange`
          // struct is as follows,
          // 1. `start_stroke`: the index of the first stroke (0-based).
          // 2. `end_stroke`: the index of the last stroke (0-based,
          // inclusive).
          // 3. `start_point`: the index of the first point in the first
          // stroke that belongs to the grapheme (0-based).
          // 4. `end_point`: the index of the last point in the last stroke
          // that belongs to the grapheme (0-based, inclusive). But for the JS
          // API, we expect the last indices to be exclusive (i.e.
          // past-the-end).
          draw_seg->begin_point_index = (stroke_idx == ink_range->start_stroke)
                                            ? ink_range->start_point
                                            : 0;
          draw_seg->end_point_index = (stroke_idx == ink_range->end_stroke)
                                          ? ink_range->end_point + 1
                                          : strokes[stroke_idx]->points.size();
          seg_blink->drawing_segments.push_back(std::move(draw_seg));
        }
      }
      prediction_blink->segmentation_result.push_back(std::move(seg_blink));
    }

    result_to_blink.push_back(std::move(prediction_blink));
  }
  std::move(callback).Run(std::move(result_to_blink));
}

}  // namespace

// static
void CrOSHandwritingRecognizerImpl::Create(
    handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback) {
  // On CrOS, only one language is supported.
  if (model_constraint->languages.size() != 1) {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kError,
        mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<chromeos::machine_learning::mojom::HandwritingRecognizer>
      cros_remote;
  auto cros_receiver = cros_remote.InitWithNewPipeAndPassReceiver();
  auto impl = base::WrapUnique(
      new CrOSHandwritingRecognizerImpl(std::move(cros_remote)));
  mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer>
      renderer_remote;
  mojo::MakeSelfOwnedReceiver<handwriting::mojom::HandwritingRecognizer>(
      std::move(impl), renderer_remote.InitWithNewPipeAndPassReceiver());

  auto model_spec =
      chromeos::machine_learning::mojom::HandwritingRecognizerSpec::New();
  model_spec->language = model_constraint->languages.front();
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModel(
          std::move(model_spec), std::move(cros_receiver),
          base::BindOnce(&OnModelBinding, std::move(renderer_remote),
                         std::move(callback)));
}

CrOSHandwritingRecognizerImpl::CrOSHandwritingRecognizerImpl(
    mojo::PendingRemote<
        chromeos::machine_learning::mojom::HandwritingRecognizer>
        pending_remote)
    : remote_cros_(std::move(pending_remote)) {}
CrOSHandwritingRecognizerImpl::~CrOSHandwritingRecognizerImpl() = default;

void CrOSHandwritingRecognizerImpl::GetPrediction(
    std::vector<handwriting::mojom::HandwritingStrokePtr> strokes,
    handwriting::mojom::HandwritingHintsPtr hints,
    GetPredictionCallback callback) {
  auto query =
      chromeos::machine_learning::mojom::HandwritingRecognitionQuery::New();
  for (const auto& stroke : strokes) {
    auto ink_stroke = chromeos::machine_learning::mojom::InkStroke::New();
    for (const auto& point : stroke->points) {
      auto ink_point = chromeos::machine_learning::mojom::InkPoint::New();
      ink_point->x = point->location.x();
      ink_point->y = point->location.y();
      ink_point->t = point->t;
      ink_stroke->points.push_back(std::move(ink_point));
    }
    query->ink.push_back(std::move(ink_stroke));
  }
  auto recognition_context =
      chromeos::machine_learning::mojom::RecognitionContext::New();
  if (!hints->text_context.empty()) {
    recognition_context->pre_context = hints->text_context;
  }
  query->context = std::move(recognition_context);
  query->max_num_results = hints->alternatives + 1;
  // We currently always enable segmentation.
  query->return_segmentation = true;
  // We currently do not support bounding box in the Web API so we do not set
  // `WritingGuide` in `query`.
  remote_cros_->Recognize(
      std::move(query), base::BindOnce(&OnRecognitionResult, std::move(strokes),
                                       std::move(callback)));
}

}  // namespace content
