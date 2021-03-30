// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/handwriting/handwriting_recognizer_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

// static
void HandwritingRecognitionServiceImpl::Create(
    mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>
        receiver) {
  mojo::MakeSelfOwnedReceiver<
      handwriting::mojom::HandwritingRecognitionService>(
      base::WrapUnique(new HandwritingRecognitionServiceImpl()),
      std::move(receiver));
}

HandwritingRecognitionServiceImpl::~HandwritingRecognitionServiceImpl() =
    default;

void HandwritingRecognitionServiceImpl::CreateHandwritingRecognizer(
    handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback) {
  HandwritingRecognizerImpl::Create(std::move(model_constraint),
                                    std::move(callback));
}

void HandwritingRecognitionServiceImpl::QueryHandwritingRecognizerSupport(
    handwriting::mojom::HandwritingFeatureQueryPtr query,
    QueryHandwritingRecognizerSupportCallback callback) {
  // By default, we do not support any handwriting recognition functionality.
  auto query_result = handwriting::mojom::HandwritingFeatureQueryResult::New();
  if (!query->languages.empty()) {
    query_result->languages =
        handwriting::mojom::HandwritingFeatureStatus::kNotSupported;
  }
  if (query->alternatives) {
    query_result->alternatives =
        handwriting::mojom::HandwritingFeatureStatus::kNotSupported;
  }
  if (query->segmentation_result) {
    query_result->segmentation_result =
        handwriting::mojom::HandwritingFeatureStatus::kNotSupported;
  }
  std::move(callback).Run(std::move(query_result));
}

HandwritingRecognitionServiceImpl::HandwritingRecognitionServiceImpl() =
    default;

}  // namespace content
