// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_android.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class PlatformSensorProviderTestAndroid : public testing::Test {
 public:
  PlatformSensorProviderTestAndroid() = default;

  void SetUp() override {
    provider_ = std::make_unique<PlatformSensorProviderAndroid>();
  }

  void CreateSensorCallback(scoped_refptr<PlatformSensor> sensor) {
    EXPECT_FALSE(sensor);
  }

 protected:
  std::unique_ptr<PlatformSensorProviderAndroid> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderTestAndroid);
};

TEST_F(PlatformSensorProviderTestAndroid, SensorManagerIsNull) {
  provider_->SetSensorManagerToNullForTesting();
  provider_->CreateSensor(
      device::mojom::SensorType::AMBIENT_LIGHT,
      base::Bind(&PlatformSensorProviderTestAndroid::CreateSensorCallback,
                 base::Unretained(this)));
}

}  // namespace device
