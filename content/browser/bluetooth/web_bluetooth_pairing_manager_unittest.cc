// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using blink::WebBluetoothDeviceId;
using blink::mojom::WebBluetoothResult;
using blink::mojom::WebBluetoothService;
using device::BluetoothDevice;

namespace content {

namespace {
constexpr int kStartingPairAttemptCount = 0;
constexpr char kValidDeviceID[] = "000000000000000000000A==";
const std::vector<uint8_t> kTestValue = {1, 2, 3, 5};
}  // namespace

class BluetoothPairingManagerTest : public testing::Test,
                                    public WebBluetoothPairingManagerDelegate {
 public:
  enum class AuthBehavior {
    kSucceedFirst,   // Successfully authenticate on first pair attempt.
    kSucceedSecond,  // Successfully authenticate on second pair attempt.
    kFailAll,        // Fail authentication on all pair attempts.
    kUnspecified,    // Initial (error) behavior.
  };

  BluetoothPairingManagerTest()
      : valid_device_id(kValidDeviceID), pairing_manager_(this) {}
  ~BluetoothPairingManagerTest() override = default;

  void SetUp() override {}

  WebBluetoothDeviceId GetCharacteristicDeviceID(
      const std::string& characteristic_instance_id) override {
    if (characteristic_instance_id == characteristic_instance_id_)
      return valid_device_id;
    return invalid_device_id;
  }

  void PairDevice(
      const WebBluetoothDeviceId& device_id,
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      base::OnceClosure callback,
      BluetoothDevice::ConnectErrorCallback error_callback) override {
    ASSERT_TRUE(device_id.IsValid());
    num_pair_attempts_++;

    switch (auth_behavior_) {
      case AuthBehavior::kSucceedFirst:
        EXPECT_EQ(1, num_pair_attempts_);
        device_paired_ = true;
        std::move(callback).Run();
        break;
      case AuthBehavior::kSucceedSecond:
        switch (num_pair_attempts_) {
          case 1:
            std::move(error_callback).Run(BluetoothDevice::ERROR_AUTH_REJECTED);
            break;
          case 2:
            device_paired_ = true;
            std::move(callback).Run();
            break;
          default:
            NOTREACHED();
            std::move(error_callback).Run(BluetoothDevice::ERROR_UNKNOWN);
        }
        break;
      case AuthBehavior::kFailAll:
        std::move(error_callback).Run(BluetoothDevice::ERROR_AUTH_REJECTED);
        break;
      case AuthBehavior::kUnspecified:
        NOTREACHED() << "Test must set auth behavior";
        break;
    }
  }

  void RemoteCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      WebBluetoothService::RemoteCharacteristicReadValueCallback callback)
      override {
    if (characteristic_instance_id != characteristic_instance_id_) {
      std::move(callback).Run(WebBluetoothResult::CHARACTERISTIC_NOT_FOUND,
                              kTestValue);
      return;
    }
    if (device_paired_) {
      std::move(callback).Run(WebBluetoothResult::SUCCESS, kTestValue);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED,
                            absl::nullopt);
  }

  const std::string& characteristic_instance_id() const {
    return characteristic_instance_id_;
  }

  const std::string& invalid_characteristic_instance_id() const {
    return invalid_characteristic_instance_id_;
  }

  void SetAuthBehavior(AuthBehavior auth_behavior) {
    auth_behavior_ = auth_behavior;
  }

  int num_pair_attempts() const { return num_pair_attempts_; }

  WebBluetoothPairingManager& pairing_manager() { return pairing_manager_; }

 private:
  int num_pair_attempts_ = 0;
  bool device_paired_ = false;
  AuthBehavior auth_behavior_ = AuthBehavior::kUnspecified;
  const std::string characteristic_instance_id_ = {"valid-id-for-tesing"};
  const std::string invalid_characteristic_instance_id_ = {"invalid-id"};
  const WebBluetoothDeviceId valid_device_id;
  const WebBluetoothDeviceId invalid_device_id;
  WebBluetoothPairingManager pairing_manager_;
};

TEST_F(BluetoothPairingManagerTest, ReadSuccessfulAuthFirstSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  base::RunLoop loop;
  pairing_manager().PairForCharacteristicReadValue(
      characteristic_instance_id(), kStartingPairAttemptCount,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, kTestValue) << "Incorrect characteristic value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadSuccessfulAuthSecondSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  base::RunLoop loop;
  pairing_manager().PairForCharacteristicReadValue(
      characteristic_instance_id(), kStartingPairAttemptCount,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, kTestValue) << "Incorrect characteristic value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadFailAllAuthsFail) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager().PairForCharacteristicReadValue(
      characteristic_instance_id(), kStartingPairAttemptCount,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManager::kMaxPairAttempts, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadInvalidCharacteristicID) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager().PairForCharacteristicReadValue(
      invalid_characteristic_instance_id(), kStartingPairAttemptCount,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
}

}  // namespace content
