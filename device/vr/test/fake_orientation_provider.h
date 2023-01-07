// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_ORIENTATION_PROVIDER_H_
#define DEVICE_VR_TEST_FAKE_ORIENTATION_PROVIDER_H_

#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/sensor.mojom.h"

namespace device {

class DEVICE_VR_EXPORT FakeOrientationSensor : public mojom::Sensor {
 public:
  FakeOrientationSensor(mojo::PendingReceiver<mojom::Sensor> receiver);

  FakeOrientationSensor(const FakeOrientationSensor&) = delete;
  FakeOrientationSensor& operator=(const FakeOrientationSensor&) = delete;

  ~FakeOrientationSensor() override;

  void AddConfiguration(const PlatformSensorConfiguration& configuration,
                        AddConfigurationCallback callback) override;
  void ConfigureReadingChangeNotifications(bool enabled) override {}

  void GetDefaultConfiguration(
      GetDefaultConfigurationCallback callback) override {}
  void RemoveConfiguration(
      const PlatformSensorConfiguration& configuration) override {}
  void Suspend() override {}
  void Resume() override {}

 private:
  mojo::Receiver<mojom::Sensor> receiver_;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_ORIENTATION_PROVIDER_H_
