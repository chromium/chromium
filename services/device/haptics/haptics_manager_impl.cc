// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/haptics/haptics_manager_impl.h"

#include <cmath>
#include <memory>
#include <utility>

#include "base/notimplemented.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

HapticsManagerImpl::HapticsManagerImpl() = default;
HapticsManagerImpl::~HapticsManagerImpl() = default;

void HapticsManagerImpl::PlayHaptics(mojom::HapticEffect effect,
                                     double intensity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!std::isfinite(intensity) || intensity < 0.0 || intensity > 1.0) {
    mojo::ReportBadMessage("Invalid haptic intensity.");
    return;
  }
  PlatformPlayHaptics(effect, intensity);
}

void HapticsManagerImpl::PlatformPlayHaptics(mojom::HapticEffect effect,
                                             double intensity) {
  NOTIMPLEMENTED();
}

// static
void HapticsManagerImpl::Create(
    mojo::PendingReceiver<mojom::HapticsService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<HapticsManagerImpl>(),
                              std::move(receiver));
}

}  // namespace device
