// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognizer_impl_cros.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace {
// Supported language tags. At the moment, CrOS only ships two models.
static constexpr char kLanguageTagEnglish[] = "en";
static constexpr char kLanguageTagGesture[] = "zxx-x-Gesture";

// Supported model identifiers. This is passed to mlservice.
static constexpr char kModelEn[] = "en";
static constexpr char kModelGesture[] = "gesture_in_context";

// Model descriptors.
handwriting::mojom::QueryHandwritingRecognizerResultPtr
CreateEnglishModelDescriptor() {
  auto desc = handwriting::mojom::QueryHandwritingRecognizerResult::New();
  desc->text_alternatives = true;
  desc->text_segmentation = true;
  desc->hints = handwriting::mojom::HandwritingHintsQueryResult::New();
  desc->hints->alternatives = true;
  desc->hints->text_context = true;
  desc->hints->recognition_type = {
      handwriting::mojom::HandwritingRecognitionType::kText};
  desc->hints->input_type = {
      handwriting::mojom::HandwritingInputType::kMouse,
      handwriting::mojom::HandwritingInputType::kStylus,
      handwriting::mojom::HandwritingInputType::kTouch,
  };

  return desc;
}

handwriting::mojom::QueryHandwritingRecognizerResultPtr
CreateGestureModelDescriptor() {
  auto desc = handwriting::mojom::QueryHandwritingRecognizerResult::New();
  desc->text_alternatives = true;
  desc->text_segmentation = false;
  desc->hints = handwriting::mojom::HandwritingHintsQueryResult::New();
  desc->hints->alternatives = true;
  desc->hints->text_context = true;
  desc->hints->recognition_type = {
      handwriting::mojom::HandwritingRecognitionType::kText};
  desc->hints->input_type = {
      handwriting::mojom::HandwritingInputType::kMouse,
      handwriting::mojom::HandwritingInputType::kStylus,
      handwriting::mojom::HandwritingInputType::kTouch,
  };
  return desc;
}

// Returns whether the two language tags are semantically the same.
// TODO(crbug.com/40742391): We may need a better language tag matching
// method (e.g. libicu's LocaleMatcher).
bool LanguageTagsAreMatching(std::string_view a, std::string_view b) {
  // Per BCP 47, language tag comparisons are case-insensitive.
  return base::EqualsCaseInsensitiveASCII(a, b);
}

// Returns the model identifier (language in HandwritingRecognizerSpec) for
// ml_service backend. Returns std::nullopt if language_tag isn't supported.
std::optional<std::string> GetModelIdentifier(std::string_view language_tag) {
  if (LanguageTagsAreMatching(language_tag, kLanguageTagEnglish))
    return kModelEn;

  if (LanguageTagsAreMatching(language_tag, kLanguageTagGesture))
    return kModelGesture;

  return std::nullopt;
}

}  // namespace

namespace content {

namespace {
using chromeos::machine_learning::mojom::LoadHandwritingModelResult;

// The callback for `mojom::MachineLearningService::LoadHandwritingModel`
// (CrOS).
void OnModelBinding(
    mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer> remote,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback,
    LoadHandwritingModelResult result) {
  if (result == LoadHandwritingModelResult::OK) {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kOk,
        std::move(remote));
    return;
  }
  switch (result) {
    case LoadHandwritingModelResult::OK:
      // Handled above.
      NOTREACHED_IN_MIGRATION();
      break;
    case LoadHandwritingModelResult::FEATURE_NOT_SUPPORTED_ERROR:
    case LoadHandwritingModelResult::LANGUAGE_NOT_SUPPORTED_ERROR:
    case LoadHandwritingModelResult::FEATURE_DISABLED_BY_USER:
    case LoadHandwritingModelResult::DLC_DOES_NOT_EXIST:
      // Report as NotSupported if MLService indicates the model isn't
      // available, or user doesn't want to use handwriting recognition.
      std::move(callback).Run(
          handwriting::mojom::CreateHandwritingRecognizerResult::kNotSupported,
          mojo::NullRemote());
      return;
    case LoadHandwritingModelResult::DLC_GET_PATH_ERROR:
    case LoadHandwritingModelResult::DLC_INSTALL_ERROR:
    case LoadHandwritingModelResult::LOAD_NATIVE_LIB_ERROR:
    case LoadHandwritingModelResult::LOAD_FUNC_PTR_ERROR:
    case LoadHandwritingModelResult::LOAD_MODEL_FILES_ERROR:
    case LoadHandwritingModelResult::LOAD_MODEL_ERROR:
    case LoadHandwritingModelResult::DEPRECATED_MODEL_SPEC_ERROR:
      // Report as error otherwise.
      std::move(callback).Run(
          handwriting::mojom::CreateHandwritingRecognizerResult::kError,
          mojo::NullRemote());
      return;
  }
}

// The callback for `mojom::HandwritingRecognizer::Recognize` (CrOS).
void OnRecognitionResult(
    CrOSHandwritingRecognizerImpl::GetPredictionCallback callback,
    std::optional<std::vector<chromeos::machine_learning::web_platform::mojom::
                                  HandwritingPredictionPtr>>
        result_from_mlservice) {
  if (!result_from_mlservice.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::vector<handwriting::mojom::HandwritingPredictionPtr> result_to_blink;
  for (const auto& prediction_ml : result_from_mlservice.value()) {
    auto prediction_blink = handwriting::mojom::HandwritingPrediction::New();
    prediction_blink->text = prediction_ml->text;
    for (const auto& segment_ml : prediction_ml->segmentation_result) {
      auto segment_blink = handwriting::mojom::HandwritingSegment::New();
      segment_blink->grapheme = segment_ml->grapheme;
      segment_blink->begin_index = segment_ml->begin_index;
      segment_blink->end_index = segment_ml->end_index;
      for (const auto& drawing_segment_ml : segment_ml->drawing_segments) {
        auto drawing_segment_blink =
            handwriting::mojom::HandwritingDrawingSegment::New();
        drawing_segment_blink->stroke_index = drawing_segment_ml->stroke_index;
        drawing_segment_blink->begin_point_index =
            drawing_segment_ml->begin_point_index;
        drawing_segment_blink->end_point_index =
            drawing_segment_ml->end_point_index;
        segment_blink->drawing_segments.push_back(
            std::move(drawing_segment_blink));
      }
      prediction_blink->segmentation_result.push_back(std::move(segment_blink));
    }
    result_to_blink.push_back(std::move(prediction_blink));
  }
  std::move(callback).Run(std::move(result_to_blink));
}

}  // namespace

// static
void CrOSHandwritingRecognizerImpl::Create(
    handwriting::mojom::HandwritingModelConstraintPtr constraint_blink,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback) {
  // On CrOS, only one language is supported.
  if (constraint_blink->languages.size() != 1) {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kNotSupported,
        mojo::NullRemote());
    return;
  }

  std::optional<std::string> model_spec_language =
      GetModelIdentifier(constraint_blink->languages[0]);
  if (!model_spec_language) {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kNotSupported,
        mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<
      chromeos::machine_learning::web_platform::mojom::HandwritingRecognizer>
      cros_remote;
  auto cros_receiver = cros_remote.InitWithNewPipeAndPassReceiver();
  auto impl = base::WrapUnique(
      new CrOSHandwritingRecognizerImpl(std::move(cros_remote)));
  mojo::PendingRemote<handwriting::mojom::HandwritingRecognizer>
      renderer_remote;
  mojo::MakeSelfOwnedReceiver<handwriting::mojom::HandwritingRecognizer>(
      std::move(impl), renderer_remote.InitWithNewPipeAndPassReceiver());

  auto constraint_ml = chromeos::machine_learning::web_platform::mojom::
      HandwritingModelConstraint::New();
  constraint_ml->languages.push_back(model_spec_language.value());

  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadWebPlatformHandwritingModel(
          std::move(constraint_ml), std::move(cros_receiver),
          base::BindOnce(&OnModelBinding, std::move(renderer_remote),
                         std::move(callback)));
}

// static
bool CrOSHandwritingRecognizerImpl::SupportsLanguageTag(
    std::string_view language_tag) {
  return GetModelIdentifier(language_tag).has_value();
}

CrOSHandwritingRecognizerImpl::CrOSHandwritingRecognizerImpl(
    mojo::PendingRemote<
        chromeos::machine_learning::web_platform::mojom::HandwritingRecognizer>
        pending_remote)
    : remote_cros_(std::move(pending_remote)) {}
CrOSHandwritingRecognizerImpl::~CrOSHandwritingRecognizerImpl() = default;

void CrOSHandwritingRecognizerImpl::GetPrediction(
    std::vector<handwriting::mojom::HandwritingStrokePtr> strokes_blink,
    handwriting::mojom::HandwritingHintsPtr hints_blink,
    GetPredictionCallback callback) {
  std::vector<
      chromeos::machine_learning::web_platform::mojom::HandwritingStrokePtr>
      strokes_ml;
  for (const auto& stroke_blink : strokes_blink) {
    auto stroke_ml = chromeos::machine_learning::web_platform::mojom::
        HandwritingStroke::New();
    for (const auto& point_blink : stroke_blink->points) {
      auto point_ml = chromeos::machine_learning::web_platform::mojom::
          HandwritingPoint::New();
      point_ml->location.set_x(point_blink->location.x());
      point_ml->location.set_y(point_blink->location.y());
      point_ml->t = point_blink->t;
      stroke_ml->points.push_back(std::move(point_ml));
    }
    strokes_ml.push_back(std::move(stroke_ml));
  }

  auto hints_ml =
      chromeos::machine_learning::web_platform::mojom::HandwritingHints::New();
  hints_ml->recognition_type = hints_blink->recognition_type;
  hints_ml->input_type = hints_blink->input_type;
  hints_ml->text_context = hints_blink->text_context;
  hints_ml->alternatives = hints_blink->alternatives;

  remote_cros_->GetPrediction(
      std::move(strokes_ml), std::move(hints_ml),
      base::BindOnce(&OnRecognitionResult, std::move(callback)));
}

// static
handwriting::mojom::QueryHandwritingRecognizerResultPtr
CrOSHandwritingRecognizerImpl::GetModelDescriptor(
    handwriting::mojom::HandwritingModelConstraintPtr constraint) {
  if (!constraint) {
    // CrOS doesn't provide a default recognizer.
    return nullptr;
  }

  if (constraint->languages.size() != 1) {
    // CrOS only supports single language recognizers.
    return nullptr;
  }

  // TODO(https://crbug.com/1231900): Integrate with language packs instead of
  // returning hard-coded values.
  const auto& model_identifier = GetModelIdentifier(constraint->languages[0]);
  if (model_identifier == kModelEn) {
    return CreateEnglishModelDescriptor();
  } else if (model_identifier == kModelGesture) {
    return CreateGestureModelDescriptor();
  } else {
    return nullptr;
  }
}

}  // namespace content
