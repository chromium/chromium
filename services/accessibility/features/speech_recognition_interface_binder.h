// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_SPEECH_RECOGNITION_INTERFACE_BINDER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_SPEECH_RECOGNITION_INTERFACE_BINDER_H_

#include "services/accessibility/features/interface_binder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-forward.h"

namespace ax {

// Binds one end of a mojom SpeechRecognition pipe hosted in Javascript to the
// AccessibilityServiceClient that connects back to the main OS process.
class SpeechRecognitionInterfaceBinder : public InterfaceBinder {
 public:
  explicit SpeechRecognitionInterfaceBinder(
      mojom::AccessibilityServiceClient* ax_service_client);
  ~SpeechRecognitionInterfaceBinder() override;
  SpeechRecognitionInterfaceBinder(const SpeechRecognitionInterfaceBinder&) =
      delete;
  SpeechRecognitionInterfaceBinder& operator=(
      const SpeechRecognitionInterfaceBinder&) = delete;

  // InterfaceBinder:
  bool MatchesInterface(const std::string& interface_name) override;
  void BindReceiver(mojo::GenericPendingReceiver srp_receiver) override;

 private:
  // The caller must ensure the client outlives `this`. Here, this is guaranteed
  // because the client is always a `AssistiveTechnologyControllerImpl`, which
  // transitively owns `this` via `V8Manager`.
  raw_ptr<mojom::AccessibilityServiceClient> ax_service_client_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_SPEECH_RECOGNITION_INTERFACE_BINDER_H_
