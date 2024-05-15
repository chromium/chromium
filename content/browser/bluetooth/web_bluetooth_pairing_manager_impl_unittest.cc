// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_pairing_manager_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::base::test::SingleThreadTaskEnvironment;
using ::base::test::TestFuture;
using ::blink::WebBluetoothDeviceId;
using ::blink::mojom::WebBluetoothResult;
using ::blink::mojom::WebBluetoothService;
using ::blink::mojom::WebBluetoothWriteType;
using ::device::BluetoothDevice;
using ::device::MockBluetoothDevice;
using ::testing::Return;
using PairPromptResult = BluetoothDelegate::PairPromptResult;

/**
 * A collection of related Bluetooth test data.
 */
struct TestData {
  const std::string characteristic_instance_id;
  const std::string descriptor_instance_id;
  const WebBluetoothDeviceId device_id;
  const std::string device_address;
  const std::string device_name;
  const std::string pincode;
};

// Test instance data that are valid and are the ID's of the fake objects
// managed by these tests.
const TestData kValidTestData = {
    .characteristic_instance_id = "valid-test-characteristic-id",
    .descriptor_instance_id = "valid-test-descriptor-id",
    .device_id = WebBluetoothDeviceId("000000000000000000000A=="),
    .device_address = "00:11:22:33:44:55",
    .device_name = "test device",
    .pincode = "123456",
};

// All valid data, but does not represent any object managed by these tests.
const TestData kValidNonTestData = {
    .characteristic_instance_id = "valid-non-test-characteristic-id",
    .descriptor_instance_id = "valid-non-test-descriptor-id",
    .device_id = WebBluetoothDeviceId("111111111111111111111A=="),
    .device_address = "66:77:88:99:00:11",
    .device_name = "non test device",
    .pincode = "789012",
};

const TestData kInvalidTestData;

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
        pairing_manager_(
            std::make_unique<WebBluetoothPairingManagerImpl>(this)) {}
  ~BluetoothPairingManagerTest() override = default;

  void SetUp() override {}

  WebBluetoothDeviceId GetCharacteristicDeviceID(
      const std::string& characteristic_instance_id) override {
    if (characteristic_instance_id == kValidTestData.characteristic_instance_id)
      return kValidTestData.device_id;
    return kInvalidTestData.device_id;
  }

  WebBluetoothDeviceId GetDescriptorDeviceId(
      const std::string& descriptor_instance_id) override {
    if (descriptor_instance_id == kValidTestData.descriptor_instance_id)
      return kValidTestData.device_id;
    return kInvalidTestData.device_id;
  }

  WebBluetoothDeviceId GetWebBluetoothDeviceId(
      const std::string& device_address) override {
    if (device_address == kValidTestData.device_address)
      return kValidTestData.device_id;
    return kInvalidTestData.device_id;
  }

  void PairDevice(const WebBluetoothDeviceId& device_id,
                  BluetoothDevice::PairingDelegate* pairing_delegate,
                  BluetoothDevice::ConnectCallback callback) override {
    ASSERT_TRUE(device_id.IsValid());
    EXPECT_EQ(device_id, kValidTestData.device_id);
    num_pair_attempts_++;

    switch (auth_behavior_) {
      case AuthBehavior::kSucceedFirst:
        EXPECT_EQ(1, num_pair_attempts_);
        device_paired_ = true;
        std::move(callback).Run(/*error_code=*/std::nullopt);
        break;
      case AuthBehavior::kSucceedSecond:
        switch (num_pair_attempts_) {
          case 1:
            std::move(callback).Run(BluetoothDevice::ERROR_AUTH_REJECTED);
            break;
          case 2:
            device_paired_ = true;
            std::move(callback).Run(/*error_code=*/std::nullopt);
            break;
          default:
            NOTREACHED_IN_MIGRATION();
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
          std::move(callback).Run(/*error_code=*/std::nullopt);
        }
        break;
      case AuthBehavior::kUnspecified:
        NOTREACHED_IN_MIGRATION() << "Test must set auth behavior";
        break;
    }
  }

  void CancelPairing(const WebBluetoothDeviceId& device_id) override {
    ASSERT_TRUE(device_id.IsValid());
    EXPECT_EQ(device_id, kValidTestData.device_id);
    std::move(pair_callback_).Run(BluetoothDevice::ERROR_AUTH_CANCELED);
  }

  void SetPinCode(const blink::WebBluetoothDeviceId& device_id,
                  const std::string& pincode) override {
    ASSERT_TRUE(device_id.IsValid());
    EXPECT_EQ(device_id, kValidTestData.device_id);
    std::move(pair_callback_).Run(/*error_code=*/std::nullopt);
  }

  void PairConfirmed(const blink::WebBluetoothDeviceId& device_id) override {
    ASSERT_TRUE(device_id.IsValid());
    EXPECT_EQ(device_id, kValidTestData.device_id);
    std::move(pair_callback_).Run(/*error_code=*/std::nullopt);
  }

  void ResumeSuspendedPairingWithSuccess() {
    device_paired_ = true;
    EXPECT_FALSE(pair_callback_.is_null());
    std::move(pair_callback_).Run(/*error_code=*/std::nullopt);
  }

  void RemoteCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      WebBluetoothService::RemoteCharacteristicReadValueCallback callback)
      override {
    if (characteristic_instance_id !=
        kValidTestData.characteristic_instance_id) {
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
                            std::nullopt);
  }

  void RemoteDescriptorReadValue(
      const std::string& descriptor_instance_id,
      WebBluetoothService::RemoteDescriptorReadValueCallback callback)
      override {
    if (descriptor_instance_id != kValidTestData.descriptor_instance_id) {
      std::move(callback).Run(WebBluetoothResult::DESCRIPTOR_NOT_FOUND,
                              descriptor_value_);
      return;
    }
    if (device_paired_) {
      std::move(callback).Run(WebBluetoothResult::SUCCESS, descriptor_value_);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED,
                            std::nullopt);
  }

  void RemoteCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      WebBluetoothWriteType write_type,
      WebBluetoothService::RemoteCharacteristicWriteValueCallback callback)
      override {
    if (characteristic_instance_id !=
        kValidTestData.characteristic_instance_id) {
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
    if (descriptor_instance_id != kValidTestData.descriptor_instance_id) {
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

  void RemoteCharacteristicStartNotificationsInternal(
      const std::string& characteristic_instance_id,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client,
      blink::mojom::WebBluetoothService::
          RemoteCharacteristicStartNotificationsCallback callback) override {
    if (characteristic_instance_id !=
        kValidTestData.characteristic_instance_id) {
      std::move(callback).Run(WebBluetoothResult::CHARACTERISTIC_NOT_FOUND);
      return;
    }
    if (device_paired_) {
      std::move(callback).Run(WebBluetoothResult::SUCCESS);
      return;
    }

    std::move(callback).Run(WebBluetoothResult::CONNECT_AUTH_REJECTED);
  }

  void PromptForBluetoothPairing(
      const std::u16string& device_identifier,
      BluetoothDelegate::PairPromptCallback callback,
      BluetoothDelegate::PairingKind pairing_kind,
      const std::optional<std::u16string>& pin) override {
    std::move(callback).Run(prompt_result_);
  }

  void SetAuthBehavior(AuthBehavior auth_behavior) {
    auth_behavior_ = auth_behavior;
  }

  int num_pair_attempts() const { return num_pair_attempts_; }

  void SetPromptResult(PairPromptResult result) {
    prompt_result_ = result;
  }

  const std::vector<uint8_t>& characteristic_value() const {
    return characteristic_value_;
  }

  const std::vector<uint8_t>& descriptor_value() const {
    return descriptor_value_;
  }

  void DeletePairingManager() { pairing_manager_.reset(); }

  WebBluetoothPairingManagerImpl* pairing_manager() {
    return pairing_manager_.get();
  }

 protected:
  BluetoothDevice::ConnectCallback pair_callback_;

 private:
  std::vector<uint8_t> characteristic_value_;
  std::vector<uint8_t> descriptor_value_;
  int num_pair_attempts_ = 0;
  bool device_paired_ = false;
  AuthBehavior auth_behavior_ = AuthBehavior::kUnspecified;
  PairPromptResult prompt_result_;
  std::unique_ptr<WebBluetoothPairingManagerImpl> pairing_manager_;
  SingleThreadTaskEnvironment single_threaded_task_environment_;
};

TEST_F(BluetoothPairingManagerTest, ReadSuccessfulAuthFirstSuccess) {
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> expected_value = characteristic_value();
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      kValidTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value)
                << "Incorrect characteristic value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadSuccessfulAuthSecondSuccess) {
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> expected_value = characteristic_value();
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      kValidTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value)
                << "Incorrect characteristic value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadFailAllAuthsFail) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      kValidTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManagerImpl::kMaxPairAttempts,
            num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadInvalidCharacteristicId) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      kValidNonTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadCharacteristicDeleteDelegate) {
  SetAuthBehavior(AuthBehavior::kSuspend);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicReadValue(
      kValidTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const std::optional<std::vector<uint8_t>>& value) {
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
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  int callback_count = 0;
  base::RunLoop loop;
  const std::vector<uint8_t> expected_value = characteristic_value();
  pairing_manager()->PairForCharacteristicReadValue(
      kValidTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop, &callback_count, &expected_value](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value);
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for reading a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForCharacteristicReadValue(
      kValidTestData.characteristic_instance_id,
      base::BindLambdaForTesting(
          [&loop, &callback_count](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
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
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, characteristic_value());

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      kValidTestData.characteristic_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, characteristic_value());

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      kValidTestData.characteristic_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_characteristic_value =
      characteristic_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_characteristic_value);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      kValidTestData.characteristic_instance_id, kWriteValue,
      WebBluetoothWriteType::kWriteWithResponse,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManagerImpl::kMaxPairAttempts,
            num_pair_attempts());
  EXPECT_EQ(default_test_characteristic_value, characteristic_value());
}

TEST_F(BluetoothPairingManagerTest, WriteCharacteristicInvalidID) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_characteristic_value =
      characteristic_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_characteristic_value);

  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      kValidNonTestData.characteristic_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      kValidTestData.characteristic_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  int callback_count = 0;
  base::RunLoop loop;
  pairing_manager()->PairForCharacteristicWriteValue(
      kValidTestData.characteristic_instance_id, kWriteValue,
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
      kValidTestData.characteristic_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> expected_value = descriptor_value();
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      kValidTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value) << "Incorrect descriptor value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadSuccessfulAuthSecondSuccess) {
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> expected_value = descriptor_value();
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      kValidTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop, &expected_value](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value) << "Incorrect descriptor value";
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadFailAllAuthsFail) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      kValidTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
            EXPECT_FALSE(value.has_value());
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManagerImpl::kMaxPairAttempts,
            num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, DescriptorReadInvalidDescriptorId) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      kValidNonTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
            loop.Quit();
          }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, ReadDescriptorDeleteDelegate) {
  SetAuthBehavior(AuthBehavior::kSuspend);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      kValidTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop](WebBluetoothResult result,
                  const std::optional<std::vector<uint8_t>>& value) {
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
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  const std::vector<uint8_t> expected_value = descriptor_value();
  int callback_count = 0;
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorReadValue(
      kValidTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop, &callback_count, &expected_value](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            EXPECT_EQ(value, expected_value) << "Incorrect descriptor value";
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for reading a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForDescriptorReadValue(
      kValidTestData.descriptor_instance_id,
      base::BindLambdaForTesting(
          [&loop, &callback_count](
              WebBluetoothResult result,
              const std::optional<std::vector<uint8_t>>& value) {
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
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, descriptor_value());

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      kValidTestData.descriptor_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, descriptor_value());

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      kValidTestData.descriptor_instance_id, kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
  EXPECT_EQ(kWriteValue, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorFailAllAuthsFail) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_descriptor_value = descriptor_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_descriptor_value);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      kValidTestData.descriptor_instance_id, kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManagerImpl::kMaxPairAttempts,
            num_pair_attempts());
  EXPECT_EQ(default_test_descriptor_value, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorInvalidID) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  const std::vector<uint8_t> default_test_descriptor_value = descriptor_value();

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  EXPECT_NE(kWriteValue, default_test_descriptor_value);

  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      kValidNonTestData.descriptor_instance_id, kWriteValue,
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_UNKNOWN_ERROR, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(0, num_pair_attempts());
  EXPECT_EQ(default_test_descriptor_value, descriptor_value());
}

TEST_F(BluetoothPairingManagerTest, WriteDescriptorDeleteDelegate) {
  SetAuthBehavior(AuthBehavior::kSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      kValidTestData.descriptor_instance_id, kWriteValue,
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
  SetAuthBehavior(AuthBehavior::kFirstSuspend);

  const std::vector<uint8_t> kWriteValue = {8, 9, 10, 11};
  int callback_count = 0;
  base::RunLoop loop;
  pairing_manager()->PairForDescriptorWriteValue(
      kValidTestData.descriptor_instance_id, kWriteValue,
      base::BindLambdaForTesting(
          [&loop, &callback_count](WebBluetoothResult result) {
            EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
            if (++callback_count == 2)
              loop.Quit();
          }));

  // Now try to pair for writing a second time. This should fail due to an
  // in-progress pairing.
  pairing_manager()->PairForDescriptorWriteValue(
      kValidTestData.descriptor_instance_id, kWriteValue,
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

TEST_F(BluetoothPairingManagerTest, CredentialPromptPINSuccess) {
  BluetoothDelegate::PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kSuccess;
  result.pin = kValidTestData.pincode;
  SetPromptResult(result);

  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, GetAddress());
  EXPECT_CALL(device, GetNameForDisplay());

  base::RunLoop run_loop;
  pair_callback_ = base::BindLambdaForTesting(
      [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
        EXPECT_FALSE(error_code);
        run_loop.Quit();
      });
  pairing_manager()->RequestPinCode(&device);
  run_loop.Run();
}

TEST_F(BluetoothPairingManagerTest, CredentialPromptPINCancelled) {
  PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kCancelled;
  SetPromptResult(result);

  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, GetAddress());
  EXPECT_CALL(device, GetNameForDisplay());

  base::RunLoop run_loop;
  pair_callback_ = base::BindLambdaForTesting(
      [&run_loop](std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
        EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, *error_code);
        run_loop.Quit();
      });
  pairing_manager()->RequestPinCode(&device);
  run_loop.Run();
}

TEST_F(BluetoothPairingManagerTest, CredentialPromptPasskeyCancelled) {
  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, CancelPairing());

  // Passkey not supported. Verify pairing cancelled.
  pairing_manager()->RequestPasskey(&device);
}

TEST_F(BluetoothPairingManagerTest, PairConfirmPromptSuccess) {
  PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kSuccess;
  SetPromptResult(result);

  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, GetAddress());
  EXPECT_CALL(device, GetNameForDisplay());

  TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>> future;
  pair_callback_ = future.GetCallback();
  pairing_manager()->AuthorizePairing(&device);
  EXPECT_FALSE(future.Get());
}

TEST_F(BluetoothPairingManagerTest, PairConfirmPromptCancelled) {
  PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kCancelled;
  SetPromptResult(result);

  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, GetAddress());
  EXPECT_CALL(device, GetNameForDisplay());

  TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>> future;
  pair_callback_ = future.GetCallback();
  pairing_manager()->AuthorizePairing(&device);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, future.Get());
}

TEST_F(BluetoothPairingManagerTest, PairConfirmPinPromptSuccess) {
  PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kSuccess;
  SetPromptResult(result);

  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, GetAddress());
  EXPECT_CALL(device, GetNameForDisplay());

  TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>> future;
  pair_callback_ = future.GetCallback();
  pairing_manager()->ConfirmPasskey(&device, 123456);
  EXPECT_FALSE(future.Get());
}

TEST_F(BluetoothPairingManagerTest, PairConfirmPinPromptCancelled) {
  PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kCancelled;
  SetPromptResult(result);

  MockBluetoothDevice device(/*adapter=*/nullptr,
                             /*bluetooth_class=*/0,
                             kValidTestData.device_name.c_str(),
                             kValidTestData.device_address,
                             /*initially_paired=*/false,
                             /*connected=*/true);

  EXPECT_CALL(device, GetAddress());
  EXPECT_CALL(device, GetNameForDisplay());

  TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>> future;
  pair_callback_ = future.GetCallback();
  pairing_manager()->ConfirmPasskey(&device, 123456);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, future.Get());
}

TEST_F(BluetoothPairingManagerTest, StartNotificationsAllAuthsSuccess) {
  SetAuthBehavior(AuthBehavior::kSucceedFirst);

  base::RunLoop loop;
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient> client;
  pairing_manager()->PairForCharacteristicStartNotifications(
      kValidTestData.characteristic_instance_id, std::move(client),
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(1, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, StartNotificationsAuthSecondSuccess) {
  SetAuthBehavior(AuthBehavior::kSucceedSecond);

  base::RunLoop loop;
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient> client;
  pairing_manager()->PairForCharacteristicStartNotifications(
      kValidTestData.characteristic_instance_id, std::move(client),
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::SUCCESS, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(2, num_pair_attempts());
}

TEST_F(BluetoothPairingManagerTest, StartNotificationsAllAuthsFail) {
  SetAuthBehavior(AuthBehavior::kFailAll);

  base::RunLoop loop;
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient> client;
  pairing_manager()->PairForCharacteristicStartNotifications(
      kValidTestData.characteristic_instance_id, std::move(client),
      base::BindLambdaForTesting([&loop](WebBluetoothResult result) {
        EXPECT_EQ(WebBluetoothResult::CONNECT_AUTH_REJECTED, result);
        loop.Quit();
      }));

  loop.Run();
  EXPECT_EQ(WebBluetoothPairingManagerImpl::kMaxPairAttempts,
            num_pair_attempts());
}

}  // namespace content
