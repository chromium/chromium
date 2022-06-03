// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_SENSOR_PROVIDER_H_
#define DEVICE_VR_TEST_FAKE_SENSOR_PROVIDER_H_

#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

class DEVICE_VR_EXPORT FakeXRSensorProvider : public mojom::SensorProvider {
 public:
  FakeXRSensorProvider();
  explicit FakeXRSensorProvider(
      mojo::PendingReceiver<mojom::SensorProvider> receiver);
  ~FakeXRSensorProvider() override;

  void Bind(mojo::PendingReceiver<mojom::SensorProvider> receiver);
  void GetSensor(mojom::SensorType type, GetSensorCallback callback) override;
  void CallCallback(mojom::SensorInitParamsPtr param);

 private:
  mojo::Receiver<mojom::SensorProvider> receiver_{this};
  GetSensorCallback callback_;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_SENSOR_PROVIDER_H_
