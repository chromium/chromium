// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_TTS_INTERFACE_BINDER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_TTS_INTERFACE_BINDER_H_

#include "services/accessibility/features/interface_binder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-forward.h"

namespace ax {

// Binds one end of a mojom TTS pipe hosted in Javascript to the
// AccessibilityServiceClient that connects back to the main OS process.
class TtsInterfaceBinder : public InterfaceBinder {
 public:
  explicit TtsInterfaceBinder(
      mojom::AccessibilityServiceClient* ax_service_client);
  ~TtsInterfaceBinder() override;
  TtsInterfaceBinder(const TtsInterfaceBinder&) = delete;
  TtsInterfaceBinder& operator=(const TtsInterfaceBinder&) = delete;

  // InterfaceBinder:
  bool MatchesInterface(const std::string& interface_name) override;
  void BindReceiver(mojo::GenericPendingReceiver tts_receiver) override;

 private:
  // The caller must ensure the client outlives `this`. Here, this is guaranteed
  // because the client is always a `AssistiveTechnologyControllerImpl`, which
  // transitively owns `this` via `V8Manager`.
  raw_ptr<mojom::AccessibilityServiceClient> ax_service_client_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_TTS_INTERFACE_BINDER_H_
