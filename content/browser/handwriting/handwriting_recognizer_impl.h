// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNIZER_IMPL_H_
#define CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNIZER_IMPL_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

// Implements the default behavior of APIs of `mojom::HandwritingRecognizer`
// between browser and renderer. The handwriting recognition service creates
// instances of a specific recognizer and passes it back to the requesting
// frame. There may be multiple recognition instances, e.g. for different
// languages.
// This class will not return any prediction. But it has the ability to
// set up and hold a mojo pipe with renderer, which can be used by the derived
// classes.
class HandwritingRecognizerImpl
    : public handwriting::mojom::HandwritingRecognizer {
 public:
  // The interface to create an object, called by handwriting service.
  static void Create(
      handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
      handwriting::mojom::HandwritingRecognitionService::
          CreateHandwritingRecognizerCallback callback);

  HandwritingRecognizerImpl(const HandwritingRecognizerImpl&) = delete;
  HandwritingRecognizerImpl& operator=(const HandwritingRecognizerImpl&) =
      delete;
  ~HandwritingRecognizerImpl() override;

 protected:
  HandwritingRecognizerImpl();

 private:
  // handwriting::mojom::HandwritingRecognizer
  // This function does not return any prediction.
  void GetPrediction(
      std::vector<handwriting::mojom::HandwritingStrokePtr> strokes,
      handwriting::mojom::HandwritingHintsPtr hints,
      GetPredictionCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNIZER_IMPL_H_
