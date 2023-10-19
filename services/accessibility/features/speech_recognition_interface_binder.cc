// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/speech_recognition_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom.h"

namespace ax {

SpeechRecognitionInterfaceBinder::SpeechRecognitionInterfaceBinder(
    mojom::AccessibilityServiceClient* ax_service_client)
    : ax_service_client_(ax_service_client) {}

SpeechRecognitionInterfaceBinder::~SpeechRecognitionInterfaceBinder() = default;

bool SpeechRecognitionInterfaceBinder::MatchesInterface(
    const std::string& interface_name) {
  return interface_name == "ax.mojom.SpeechRecognition";
}

void SpeechRecognitionInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver sr_receiver) {
  ax_service_client_->BindSpeechRecognition(
      sr_receiver.As<ax::mojom::SpeechRecognition>());
}

}  // namespace ax
