// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/tts_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"

namespace ax {

TtsInterfaceBinder::TtsInterfaceBinder(
    mojom::AccessibilityServiceClient* ax_service_client)
    : ax_service_client_(ax_service_client) {}

TtsInterfaceBinder::~TtsInterfaceBinder() = default;

bool TtsInterfaceBinder::MatchesInterface(const std::string& interface_name) {
  return interface_name == "ax.mojom.Tts";
}

void TtsInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver tts_receiver) {
  ax_service_client_->BindTts(tts_receiver.As<ax::mojom::Tts>());
}

}  // namespace ax
