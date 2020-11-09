// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_winrt.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

// Mock for PlatformSensorReaderWinBase, used to ensure the actual
// Windows.Devices.Sensor API isn't called.
class MockSensorReader : public PlatformSensorReaderWinBase {
 public:
  MockSensorReader() = default;
  ~MockSensorReader() override = default;

  MOCK_METHOD1(SetClient, void(Client* client));
  MOCK_CONST_METHOD0(GetMinimalReportingInterval, base::TimeDelta());
  MOCK_METHOD1(StartSensor,
               bool(const PlatformSensorConfiguration& configuration));
  MOCK_METHOD0(StopSensor, void());
};

// Mock for SensorReaderFactory, injected into
// PlatformSensorProviderWinrt so it will create MockSensorReaders instead
// of the real PlatformSensorReaderWinBase which calls into the WinRT APIs.
class MockSensorReaderFactory : public SensorReaderFactory {
 public:
  MockSensorReaderFactory() = default;
  ~MockSensorReaderFactory() override = default;

  MOCK_METHOD1(
      CreateSensorReader,
      std::unique_ptr<PlatformSensorReaderWinBase>(mojom::SensorType type));
};

// Tests that PlatformSensorProviderWinrt can successfully be instantiated
// and passes the correct result to the CreateSensor callback.
TEST(PlatformSensorProviderTestWinrt, SensorCreationReturnCheck) {
  base::test::TaskEnvironment task_environment;

  auto mock_sensor_reader_factory =
      std::make_unique<testing::NiceMock<MockSensorReaderFactory>>();

  // Return valid PlatformSensorReaderWinBase instances for gyroscope and
  // accelerometer to represent them as supported/present. Return nullptr
  // for ambient light to represent it as not present/supported.
  EXPECT_CALL(*mock_sensor_reader_factory.get(),
              CreateSensorReader(mojom::SensorType::AMBIENT_LIGHT))
      .WillOnce(testing::Invoke([](mojom::SensorType) { return nullptr; }));
  EXPECT_CALL(*mock_sensor_reader_factory.get(),
              CreateSensorReader(mojom::SensorType::GYROSCOPE))
      .WillOnce(testing::Invoke([](mojom::SensorType) {
        return std::make_unique<testing::NiceMock<MockSensorReader>>();
      }));
  EXPECT_CALL(*mock_sensor_reader_factory.get(),
              CreateSensorReader(mojom::SensorType::ACCELEROMETER))
      .WillOnce(testing::Invoke([](mojom::SensorType) {
        return std::make_unique<testing::NiceMock<MockSensorReader>>();
      }));

  auto provider = std::make_unique<PlatformSensorProviderWinrt>();

  provider->SetSensorReaderFactoryForTesting(
      std::move(mock_sensor_reader_factory));

  // CreateSensor is async so use a RunLoop to wait for completion.
  base::Optional<base::RunLoop> run_loop;
  bool expect_sensor_valid = false;

  base::RepeatingCallback<void(scoped_refptr<PlatformSensor> sensor)>
      create_sensor_callback =
          base::BindLambdaForTesting([&](scoped_refptr<PlatformSensor> sensor) {
            if (expect_sensor_valid)
              EXPECT_TRUE(sensor);
            else
              EXPECT_FALSE(sensor);
            run_loop->Quit();
          });

  run_loop.emplace();
  provider->CreateSensor(mojom::SensorType::AMBIENT_LIGHT,
                         base::BindOnce(create_sensor_callback));
  run_loop->Run();

  expect_sensor_valid = true;
  run_loop.emplace();
  provider->CreateSensor(mojom::SensorType::GYROSCOPE,
                         base::BindOnce(create_sensor_callback));
  run_loop->Run();

  // Linear acceleration is a fusion sensor built on top of accelerometer,
  // this should trigger the CreateSensorReader(ACCELEROMETER) call.
  expect_sensor_valid = true;
  run_loop.emplace();
  provider->CreateSensor(mojom::SensorType::LINEAR_ACCELERATION,
                         base::BindOnce(create_sensor_callback));
  run_loop->Run();
}

}  // namespace device
