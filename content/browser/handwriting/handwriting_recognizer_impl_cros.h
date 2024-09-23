// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNIZER_IMPL_CROS_H_
#define CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNIZER_IMPL_CROS_H_

#include <string_view>
#include <vector>

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom-forward.h"
#include "chromeos/services/machine_learning/public/mojom/web_platform_handwriting.mojom.h"
#include "content/browser/handwriting/handwriting_recognizer_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom-forward.h"

namespace content {

// Implements the mojo API of `mojom::HandwritingRecognizer` between browser and
// renderer for CrOS.
// One renderer process can create multiple objects of this class. All
// recognition requests from renderer must go through the API here for security
// checks etc. This class will also hold a mojo remote to the mlservice daemon
// CrOS and mlservice will create a handwriting model instance for each of this
// class.
class CrOSHandwritingRecognizerImpl final : public HandwritingRecognizerImpl {
 public:
  // The interface to create an object, called by handwriting service.
  static void Create(
      handwriting::mojom::HandwritingModelConstraintPtr constraint_blink,
      handwriting::mojom::HandwritingRecognitionService::
          CreateHandwritingRecognizerCallback callback);

  CrOSHandwritingRecognizerImpl(const HandwritingRecognizerImpl&) = delete;
  CrOSHandwritingRecognizerImpl& operator=(const HandwritingRecognizerImpl&) =
      delete;
  ~CrOSHandwritingRecognizerImpl() override;

  // Returns whether the provided |language_tag| is supported.
  static bool SupportsLanguageTag(std::string_view language_tag);

  // Returns the description for the model that best satisfies `constraint`.
  // Returns nullptr if we can't find such a model.
  static handwriting::mojom::QueryHandwritingRecognizerResultPtr
  GetModelDescriptor(
      handwriting::mojom::HandwritingModelConstraintPtr constraint);

 private:
  explicit CrOSHandwritingRecognizerImpl(
      mojo::PendingRemote<chromeos::machine_learning::web_platform::mojom::
                              HandwritingRecognizer> pending_remote);

  // handwriting::mojom::HandwritingRecognizer
  void GetPrediction(
      std::vector<handwriting::mojom::HandwritingStrokePtr> strokes,
      handwriting::mojom::HandwritingHintsPtr hints,
      GetPredictionCallback callback) override;

  mojo::Remote<
      chromeos::machine_learning::web_platform::mojom::HandwritingRecognizer>
      remote_cros_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNIZER_IMPL_CROS_H_
