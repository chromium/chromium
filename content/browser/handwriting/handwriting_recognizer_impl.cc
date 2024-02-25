// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognizer_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void HandwritingRecognizerImpl::Create(
    handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback) {
  std::move(callback).Run(
      handwriting::mojom::CreateHandwritingRecognizerResult::kNotSupported,
      mojo::NullRemote());
}

HandwritingRecognizerImpl::HandwritingRecognizerImpl() = default;
HandwritingRecognizerImpl::~HandwritingRecognizerImpl() = default;

void HandwritingRecognizerImpl::GetPrediction(
    std::vector<handwriting::mojom::HandwritingStrokePtr> strokes,
    handwriting::mojom::HandwritingHintsPtr hints,
    GetPredictionCallback callback) {
  std::move(callback).Run(std::nullopt);
}

}  // namespace content
