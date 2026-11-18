// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_HAPTICS_SERVICE_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_HAPTICS_SERVICE_OVERRIDER_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/haptics.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockHapticsService : public mojom::HapticsService {
 public:
  MockHapticsService();
  MockHapticsService(const MockHapticsService&) = delete;
  MockHapticsService& operator=(const MockHapticsService&) = delete;
  ~MockHapticsService() override;

  void Bind(mojo::PendingReceiver<mojom::HapticsService> receiver);

  // mojom::HapticsService:
  MOCK_METHOD(void,
              PlayHaptics,
              (mojom::HapticEffect effect, double intensity),
              (override));

 private:
  mojo::ReceiverSet<mojom::HapticsService> receivers_;
};

class ScopedHapticsServiceOverrider {
 public:
  ScopedHapticsServiceOverrider();
  ScopedHapticsServiceOverrider(const ScopedHapticsServiceOverrider&) = delete;
  ScopedHapticsServiceOverrider& operator=(
      const ScopedHapticsServiceOverrider&) = delete;
  ~ScopedHapticsServiceOverrider();

  MockHapticsService* haptics_service() { return haptics_service_.get(); }

 private:
  std::unique_ptr<MockHapticsService> haptics_service_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_HAPTICS_SERVICE_OVERRIDER_H_
