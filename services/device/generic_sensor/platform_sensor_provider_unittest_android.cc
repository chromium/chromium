// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "services/device/generic_sensor/platform_sensor_android.h"
#include "services/device/generic_sensor/platform_sensor_provider_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class PlatformSensorProviderTestAndroid : public testing::Test {
 public:
  PlatformSensorProviderTestAndroid() = default;
  PlatformSensorProviderTestAndroid(PlatformSensorProviderTestAndroid&) =
      delete;
  PlatformSensorProviderTestAndroid& operator=(
      PlatformSensorProviderTestAndroid&) = delete;

  void SetUp() override {
    provider_ = std::make_unique<PlatformSensorProviderAndroid>();
  }

  void CreateSensorCallback(scoped_refptr<PlatformSensor> sensor) {
    EXPECT_FALSE(sensor);
  }

 protected:
  std::unique_ptr<PlatformSensorProviderAndroid> provider_;

 private:
  base::test::TaskEnvironment task_environment;
};

TEST_F(PlatformSensorProviderTestAndroid, SensorManagerIsNull) {
  provider_->SetSensorManagerToNullForTesting();
  provider_->CreateSensor(
      device::mojom::SensorType::AMBIENT_LIGHT,
      base::BindOnce(&PlatformSensorProviderTestAndroid::CreateSensorCallback,
                     base::Unretained(this)));
}

TEST_F(PlatformSensorProviderTestAndroid, SensorErrorDuringSensorDestruction) {
  base::test::TestFuture<scoped_refptr<PlatformSensor>> future;
  provider_->CreateSensor(device::mojom::SensorType::AMBIENT_LIGHT,
                          future.GetCallback());
  auto sensor = future.Take();
  EXPECT_TRUE(sensor);
  PlatformSensorAndroid* android_sensor =
      static_cast<PlatformSensorAndroid*>(sensor.get());

  base::RunLoop run_loop_for_error;
  auto sensor_java_object = android_sensor->GetJavaObjectForTesting();

  // Simulate a 0 length SensorEvent which will trigger
  // PlatformSensor::sensorError().
  base::ThreadPool::PostTask(
      FROM_HERE, {}, base::BindLambdaForTesting([&]() {
        PlatformSensorAndroid::SimulateSensorEventFromJavaForTesting(
            sensor_java_object, 0);
        run_loop_for_error.Quit();
      }));
  // Sleep 1ms to yield the thread for the other thread a chance to run.
  base::PlatformThread::Sleep(base::Milliseconds(1));
  sensor.reset();
  run_loop_for_error.Run();
}

}  // namespace device
