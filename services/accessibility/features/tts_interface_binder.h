// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_TTS_INTERFACE_BINDER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_TTS_INTERFACE_BINDER_H_

#include "services/accessibility/features/interface_binder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

// Binds one end of a mojom TTS pipe hosted in Javascript to the
// AccessibilityServiceClient that connects back to the main OS process.
class TtsInterfaceBinder : public InterfaceBinder {
 public:
  TtsInterfaceBinder(
      base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client,
      scoped_refptr<base::SequencedTaskRunner> main_runner);
  ~TtsInterfaceBinder() override;
  TtsInterfaceBinder(const TtsInterfaceBinder&) = delete;
  TtsInterfaceBinder& operator=(const TtsInterfaceBinder&) = delete;

  // InterfaceBinder:
  bool MatchesInterface(const std::string& interface_name) override;
  void BindReceiver(mojo::GenericPendingReceiver tts_receiver) override;

 private:
  base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client_;
  scoped_refptr<base::SequencedTaskRunner> main_runner_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_TTS_INTERFACE_BINDER_H_
