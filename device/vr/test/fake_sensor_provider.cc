// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_sensor_provider.h"

#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

FakeXRSensorProvider::FakeXRSensorProvider() = default;

FakeXRSensorProvider::FakeXRSensorProvider(
    mojo::PendingReceiver<mojom::SensorProvider> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeXRSensorProvider::~FakeXRSensorProvider() {
  if (callback_) {
    std::move(callback_).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                             nullptr);
  }
}

void FakeXRSensorProvider::Bind(
    mojo::PendingReceiver<mojom::SensorProvider> receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeXRSensorProvider::GetSensor(mojom::SensorType type,
                                     GetSensorCallback callback) {
  callback_ = std::move(callback);
}

void FakeXRSensorProvider::CallCallback(mojom::SensorInitParamsPtr param) {
  std::move(callback_).Run(mojom::SensorCreationResult::SUCCESS,
                           std::move(param));
}

}  // namespace device
