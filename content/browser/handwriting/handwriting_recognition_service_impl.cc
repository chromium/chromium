// Copyright 2021 The Chromium Authors
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

void HandwritingRecognitionServiceImpl::QueryHandwritingRecognizer(
    handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
    QueryHandwritingRecognizerCallback callback) {
  // By default, we don't support any handwriting recognizer.
  std::move(callback).Run(nullptr);
}

HandwritingRecognitionServiceImpl::HandwritingRecognitionServiceImpl() =
    default;

}  // namespace content
