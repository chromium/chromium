// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

// Implements the default behavior of the mojo APIs of
// `handwriting::mojom::HandwritingRecognitionService`. At most one per frame in
// the renderer process.
// This class does not support any handwriting
// functionality. But it has the ability bootstrap and hold the mojo connection
// to renderer, which can be reused by the derived class.
class CONTENT_EXPORT HandwritingRecognitionServiceImpl
    : public handwriting::mojom::HandwritingRecognitionService {
 public:
  ~HandwritingRecognitionServiceImpl() override;

  // The interface to create an object, called by the handwriting factory.
  static void Create(
      mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>
          receiver);

  HandwritingRecognitionServiceImpl(const HandwritingRecognitionServiceImpl&) =
      delete;
  HandwritingRecognitionServiceImpl& operator=(
      const HandwritingRecognitionServiceImpl&) = delete;

 protected:
  HandwritingRecognitionServiceImpl();

 private:
  // handwriting::mojom::HandwritingRecognitionService
  void CreateHandwritingRecognizer(
      handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
      CreateHandwritingRecognizerCallback callback) override;

  // Always return `nullptr`.
  void QueryHandwritingRecognizer(
      handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
      QueryHandwritingRecognizerCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_IMPL_H_
