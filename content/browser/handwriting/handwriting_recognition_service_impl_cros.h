// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_IMPL_CROS_H_
#define CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_IMPL_CROS_H_

#include "content/browser/handwriting/handwriting_recognition_service_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

// Implements the mojo API of handwriting::mojom::HandwritingRecognitionService
// for CrOS. Each renderer process can only have one such object.
class CONTENT_EXPORT CrOSHandwritingRecognitionServiceImpl final
    : public HandwritingRecognitionServiceImpl {
 public:
  ~CrOSHandwritingRecognitionServiceImpl() override;

  // The interface to create an object, called by the handwriting factory.
  static void Create(
      mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>);

  CrOSHandwritingRecognitionServiceImpl(
      const CrOSHandwritingRecognitionServiceImpl&) = delete;
  CrOSHandwritingRecognitionServiceImpl& operator=(
      const CrOSHandwritingRecognitionServiceImpl&) = delete;

 private:
  CrOSHandwritingRecognitionServiceImpl();

  // handwriting::mojom::HandwritingRecognitionService
  void CreateHandwritingRecognizer(
      handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
      CreateHandwritingRecognizerCallback callback) override;
  void QueryHandwritingRecognizer(
      handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
      QueryHandwritingRecognizerCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_IMPL_CROS_H_
