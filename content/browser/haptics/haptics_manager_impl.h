// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_IMPL_H_

#include "content/browser/haptics/haptics_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/haptics.mojom.h"

namespace content {

// Platform Web Haptics backend that forwards playback requests to the Device
// Service over Mojo.
class HapticsManagerImpl final : public HapticsManager {
 public:
  HapticsManagerImpl();
  HapticsManagerImpl(const HapticsManagerImpl&) = delete;
  HapticsManagerImpl& operator=(const HapticsManagerImpl&) = delete;
  ~HapticsManagerImpl() override;

  // HapticsManager:
  void PlayHaptics(device::mojom::HapticEffect effect,
                   double intensity) override;

 private:
  mojo::Remote<device::mojom::HapticsService> device_haptics_service_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HAPTICS_HAPTICS_MANAGER_IMPL_H_
