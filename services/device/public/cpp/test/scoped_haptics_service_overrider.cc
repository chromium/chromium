// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_haptics_service_overrider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "services/device/device_service.h"

namespace device {

MockHapticsService::MockHapticsService() = default;

MockHapticsService::~MockHapticsService() = default;

void MockHapticsService::Bind(
    mojo::PendingReceiver<mojom::HapticsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

ScopedHapticsServiceOverrider::ScopedHapticsServiceOverrider()
    : haptics_service_(std::make_unique<MockHapticsService>()) {
  DeviceService::OverrideHapticsServiceBinderForTesting(base::BindRepeating(
      &MockHapticsService::Bind, base::Unretained(haptics_service_.get())));
}

ScopedHapticsServiceOverrider::~ScopedHapticsServiceOverrider() {
  DeviceService::OverrideHapticsServiceBinderForTesting(base::NullCallback());
}

}  // namespace device
