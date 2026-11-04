// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HAPTICS_HAPTICS_MANAGER_IMPL_H_
#define SERVICES_DEVICE_HAPTICS_HAPTICS_MANAGER_IMPL_H_

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/haptics.mojom.h"

namespace device {

// Default Device Service implementation of the HapticsService mojo interface.
// Dispatches to platform-specific haptic actuators.
class HapticsManagerImpl : public mojom::HapticsService {
 public:
  static void Create(mojo::PendingReceiver<mojom::HapticsService> receiver);

  HapticsManagerImpl();
  HapticsManagerImpl(const HapticsManagerImpl&) = delete;
  HapticsManagerImpl& operator=(const HapticsManagerImpl&) = delete;
  ~HapticsManagerImpl() override;

  // mojom::HapticsService:
  void PlayHaptics(mojom::HapticEffect effect, double intensity) final;

 protected:
  virtual void PlatformPlayHaptics(mojom::HapticEffect effect,
                                   double intensity);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_HAPTICS_HAPTICS_MANAGER_IMPL_H_
