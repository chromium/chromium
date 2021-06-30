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
using blink::mojom::WebBluetoothWriteType;
using device::BluetoothDevice;

namespace content {

namespace {
constexpr char kValidDeviceID[] = "000000000000000000000A==";
}  // namespace

class BluetoothPairingManagerTest : public testing::Test,
                                    public WebBluetoothPairingManagerDelegate {
 public:
  enum class AuthBehavior {
    kSucceedFirst,   // Successfully authenticate on first pair attempt.
    kSucceedSecond,  // Successfully authenticate on second pair attempt.
    kFailAll,        // Fail authentication on all pair attempts.
    kSuspend,        // Suspend in-progress authentication.
    kFirstSuspend,   // First auth is suspended, following will succeed.
    kUnspecified,    // Initial (error) behavior.
  };

  BluetoothPairingManagerTest()
      : characteristic_value_{1, 2, 3, 5},
        descriptor_value_{4, 8, 15, 16, 23, 42},
        valid_device_id(kValidDeviceID),
        pairing_manager_(std::make_unique<WebBluetoothPairingManager>(this)) {}
  ~BluetoothPairingManagerTest() override = default;

  void SetUp() override {}

  WebBluetoothDeviceId GetCharacteristicDeviceID(
      const std::string& characteristic_instance_id) override {
    if (characteristic_instance_id == characteristic_instance_id_)
      return valid_device_id;
    return invalid_device_id;
  }

  WebBluetoothDeviceId GetDescriptorDeviceId(
      const std::string& descriptor_instance_id) override {
    if (descriptor_instance_id == descriptor_instance_id_)
      return valid_device_id;
    return invalid_device_id;
  }

  void PairDevice(const WebBluetoothDeviceId& device_id,
                  device::BluetoothDevice::PairingDelegate* pairing_delegate,
                  BluetoothDevice::ConnectCallback callback) override {
    ASSERT_TRUE(device_id.IsValid());
    EXPECT_EQ(device_id, valid_device_id);
    num_pair_attempts_++;

    switch (auth_behavior_) {
      case AuthBehavior::kSucceedFirst:
        EXPECT_EQ(1, num_pair_attempts_);
        device_paired_ = true;
        std::move(callback).Run(/*error_code=*/absl::nullopt);
        break;
      case AuthBehavior::kSucceedSecond:
        switch (num_pair_attempts_) {
          case 1:
            std::move(callback).Run(BluetoothDevice::ERROR_AUTH_REJECTED);
            break;
          case 2:
            device_paired_ = true;
            std::move(callback).Run(/*error_code=*/absl::nullopt);
            break;
          default:
            NOTREACHED();
            std::move(callback).Run(BluetoothDevice::ERROR_UNKNOWN);
        }
        break;
      case AuthBehavior::kFailAll:
        std::move(callback).Run(BluetoothDevice::ERROR_AUTH_REJECTED);
        break;
      case AuthBehavior::kSuspend:
        EXPECT_TRUE(pair_callback_.is_null());
        pair_callback_ = std::move(callback);
        break;
      case AuthBehavior::kFirstSuspend:
        if (num_pair_attempts_ == 1) {
          EXPECT_TRUE(pair_callback_.is_null());
          pair_callback_ = std::move(callback);
        } else {
          device_paired_ = true;
          std::move(callback).Run(/*error_code=*/absl::nullopt);
        }
        break;
      case AuthBehavior::kUnspecified:
        NOTREACHED() << "Test must set auth behavior";
        break;
    }
  }

  void CancelPairing(const WebBluetoothDeviceId& device_id) override {
    ASSERT_TRUE(device_id.IsValid());
    EXPECT_EQ(device_id, valid_device_id);
    std::move(pair_callback_).Run(BluetoothDevice::ERROR_AUTH_CANCELED);
  }

  void ResumeSuspendedPairingWithSuccess() {
    device_paired_ = true;
    EXPECT_FALSE(pair_callback_.is_null());
    std::move(pair_callback_).Run(/*error+_code=*/absl::nullopt);
  }

  void RemoteCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      WebBluetoothService::RemoteCharacteristicReadValueCallback callback)
      override {
    if (characteristic_instance_id != characteristic_instance_id_) {
      std::move(callback).Run(WebBluetoothResult::CHARACTERISTIC_NOT_FOUND,
                              characteristic_value_);
      return;
    }
    if (device_paired_) {
      std::move(callback).Run(WebBluetoothResult::SUCCESS,
                              characteristic_value_);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED,
                            absl::nullopt);
  }

  void RemoteDescriptorReadValue(
      const std::string& descriptor_instance_id,
      WebBluetoothService::RemoteDescriptorReadValueCallback callback)
      override {
    if (descriptor_instance_id != descriptor_instance_id_) {
      std::move(callback).Run(WebBluetoothResult::DESCRIPTOR_NOT_FOUND,
                              descriptor_value_);
      return;
    }
    if (device_paired_) {
      std::move(callback).Run(WebBluetoothResult::SUCCESS, descriptor_value_);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED,
                            absl::nullopt);
  }

  void RemoteCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      WebBluetoothWriteType write_type,
      WebBluetoothService::RemoteCharacteristicWriteValueCallback callback)
      override {
    if (characteristic_instance_id != characteristic_instance_id_) {
      std::move(callback).Run(WebBluetoothResult::CHARACTERISTIC_NOT_FOUND);
      return;
    }
    if (device_paired_) {
      characteristic_value_ = value;
      std::move(callback).Run(WebBluetoothResult::SUCCESS);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED);
  }

  void RemoteDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      WebBluetoothService::RemoteDescriptorWriteValueCallback callback)
      override {
    if (descriptor_instance_id != descriptor_instance_id_) {
      std::move(callback).Run(WebBluetoothResult::DESCRIPTOR_NOT_FOUND);
      return;
    }
    if (device_paired_) {
      descriptor_value_ = value;
      std::move(callback).Run(WebBluetoothResult::SUCCESS);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED);
  }

  const std::string& characteristic_instance_id() const {
    return characteristic_instance_id_;
  }

  const std::string& invalid_characteristic_instance_id() const {
    return invalid_characteristic_instance_id_;
  }

  const std::string& descriptor_instance_id() const {
    return descriptor_instance_id_;
  }

  const std::string& invalid_descriptor_instance_id() const {
    return invalid_descriptor_instance_id_;
  }

  void SetAuthBehavior(AuthBehavior auth_behavior) {
    auth_behavior_ = auth_behavior;
  }

  int num_pair_attempts() const { return num_pair_attempts_; }

  const std::vector<uint8_t>& characteristic_value() const {
    return characteristic_value_;
  }

  const std::vector<uint8_t>& descriptor_value() const {
    return descriptor_value_;
  }

  void DeletePairingManager() { pairing_manager_.reset(); }

  WebBluetoothPairingManager* pairing_manager() {
    return pairing_manager_.get();
  }

 private:
  std::vector<uint8_t> characteristic_value_;
  std::vector<uint8_t> descriptor_value_;
  int num_pair_attempts_ = 0;
  bool device_paired_ = false;
  BluetoothDevice::ConnectCallback pair_callback_;
  AuthBehavior auth_behavior_ = AuthBehavior::kUnspecified;
  const std::string characteristic_instance_id_ = "valid-id-for-tesing";
  const std::string invalid_characteristic_instance_id_ = "invalid-id";
  const std::string descriptor_instance_id_ = "valid-id-for-tesing";
  const std::string invalid_descriptor_instance_id_ = "invalid-id";
  const WebBluetoothDeviceId valid_device_id;
  const WebBluetoothDeviceId invalid_device_id;
  std::unique_ptr<WebBluetoothPairingManager> pairing_manager_;
};

TEST_F(BluetoothPairingManagerTest, ReadSuccessfulAuthFirstSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> expected_value = characteristic_value();
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value)
                << "Incorrect characteristic value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadSuccessfulAuthSecondSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> expected_value = characteristic_value();
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value)
                << "Incorrect characteristic value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadFailAllAuthsFail) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManager::kMaxPairAttempts, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadInvalidCharacteristicId) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      invalid_characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadCharacteristicDeleteDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSuspend);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  // Deleting the pairing manager will cancel all pending device pairing
  // operations. Test this is true.
  DeletePairingManager();
  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadCharacteristicDoublePair) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  int callback_count = 0;
  base::RunLoop loop;
  const std::vector<uint8_t> expected_value = characteristic_value();
  pairing_manager()->PairForCharacteristicReadValue(
      characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &callback_count, &expected_value](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value);
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for reading a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForCharacteristicReadValue(
      characteristic_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &callback_count](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
            if (++callback_count == 2)
              loop.Quit();
          }));

  ResumeSuspendedPairingWithSuccess();
  loop.Run();

  EXPECT_EQ(1, num_pair_attempts()) << "Only the first operation should pair";
}

TEST_F(BluetoothPairingManagerTest,
       WriteCharacteristicSuccessfulAuthFirstSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, characteristic_value());

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
  EXPECT_EQ(kWriteValue, characteristic_value());
}

TEST_F(BluetoothPairingManagerTest,
       WriteCharacteristicSuccessfulAuthSecondSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, characteristic_value());

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
  EXPECT_EQ(kWriteValue, characteristic_value());
}

TEST_F(BluetoothPairingManagerTest, WriteCharacteristicFailAllAuthsFail) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_characteristic_value =
      characteristic_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_characteristic_value);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManager::kMaxPairAttempts, num_pair_attempts());
  EXPECT_EQ(default_test_characteristic_value, characteristic_value());
}

TEST_F(BluetoothPairingManagerTest, WriteCharacteristicInvalidID) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_characteristic_value =
      characteristic_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_characteristic_value);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      invalid_characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
  EXPECT_EQ(default_test_characteristic_value, characteristic_value());
}

TEST_F(BluetoothPairingManagerTest, WriteCharacteristicDeleteDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
        loop.Quit();
      }));

  // Verify that deleting the pairing manager will cancel all pending device
  // pairing.
  DeletePairingManager();
  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, WriteCharacteristicDoublePair) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  int callback_count = 0;
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting(
          [&loop, &callback_count](WebBluetoothResult result) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for writing a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForCharacteristicWriteValue(
      characteristic_instance_id(), kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting(
          [&loop, &callback_count](WebBluetoothResult result) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
            if (++callback_count == 2)
              loop.Quit();
          }));

  ResumeSuspendedPairingWithSuccess();
  loop.Run();

  EXPECT_EQ(1, num_pair_attempts()) << "Only the first operation should pair";
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadSuccessfulAuthFirstSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> expected_value = descriptor_value();
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value) << "Incorrect descriptor value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadSuccessfulAuthSecondSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> expected_value = descriptor_value();
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value) << "Incorrect descriptor value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadFailAllAuthsFail) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManager::kMaxPairAttempts, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadInvalidDescriptorId) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      invalid_descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadDescriptorDeleteDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSuspend);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
            loop.Quit();
          }));

  // Verify that deleting the pairing manager will cancel all pending device
  // pairing.
  DeletePairingManager();
  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadDescriptorDoublePair) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  const std::vector<uint8_t> expected_value = descriptor_value();
  int callback_count = 0;
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &callback_count, &expected_value](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value) << "Incorrect descriptor value";
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for reading a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForDescriptorReadValue(
      descriptor_instance_id(),
      base::BindLambdaForTesting(
          [&loop, &callback_count](
              WebBluetoothResult result,
              const absl::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
            EXPECT_FALSE(value.has_value());
            if (++callback_count == 2)
              loop.Quit();
          }));

  ResumeSuspendedPairingWithSuccess();
  loop.Run();

  EXPECT_EQ(1, num_pair_attempts()) << "Only the first operation should pair";
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorSuccessfulAuthFirstSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, descriptor_value());

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
  EXPECT_EQ(kWriteValue, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest,
       WriteDescriptorSuccessfulAuthSecondSuccess) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, descriptor_value());

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
  EXPECT_EQ(kWriteValue, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorFailAllAuthsFail) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_descriptor_value = descriptor_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_descriptor_value);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManager::kMaxPairAttempts, num_pair_attempts());
  EXPECT_EQ(default_test_descriptor_value, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorInvalidID) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_descriptor_value = descriptor_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_descriptor_value);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      invalid_descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
  EXPECT_EQ(default_test_descriptor_value, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorDeleteDelegate) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
        loop.Quit();
      }));

  // Verify that deleting the pairing manager will cancel all pending device
  // pairing.
  DeletePairingManager();
  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorDoublePair) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  int callback_count = 0;
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting(
          [&loop, &callback_count](WebBluetoothResult result) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for writing a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForDescriptorWriteValue(
      descriptor_instance_id(), kWriteValue,
      base::BindLambdaForTesting(
          [&loop, &callback_count](WebBluetoothResult result) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_CANCELED, result);
            if (++callback_count == 2)
              loop.Quit();
          }));

  ResumeSuspendedPairingWithSuccess();
  loop.Run();

  EXPECT_EQ(1, num_pair_attempts()) << "Only the first operation should pair";
}

}  // namespace content
