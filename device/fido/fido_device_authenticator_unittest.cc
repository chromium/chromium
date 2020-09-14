// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/large_blob.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using WriteCallback =
    device::test::ValueCallbackReceiver<CtapDeviceResponseCode>;
using ReadCallback = device::test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    base::Optional<std::vector<std::pair<LargeBlobKey, std::vector<uint8_t>>>>>;

constexpr LargeBlobKey kDummyKey = {{0x01}};
constexpr std::array<uint8_t, 4> kSmallBlob = {'l', 'u', 'm', 'a'};
constexpr size_t kMaxStorageSize = 4096;

class FidoDeviceAuthenticatorTest : public testing::Test {
 public:
  void SetUp() override {
    VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.large_blob_support = true;
    config.resident_key_support = true;
    config.available_large_blob_storage = kMaxStorageSize;

    authenticator_state_ = base::MakeRefCounted<VirtualFidoDevice::State>();
    auto virtual_device =
        std::make_unique<VirtualCtap2Device>(authenticator_state_, config);
    authenticator_ =
        std::make_unique<FidoDeviceAuthenticator>(std::move(virtual_device));

    device::test::TestCallbackReceiver<> callback;
    authenticator_->InitializeAuthenticator(callback.callback());
    callback.WaitForCallback();
  }

 protected:
  void SetPin() {
    authenticator_state_->pin = "1234";
    authenticator_state_->pin_retries = device::kMaxPinRetries;
  }

  scoped_refptr<VirtualFidoDevice::State> authenticator_state_;
  std::unique_ptr<FidoDeviceAuthenticator> authenticator_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FidoDeviceAuthenticatorTest, TestReadEmptyLargeBlob) {
  ReadCallback callback;
  authenticator_->ReadLargeBlob({kDummyKey}, base::nullopt,
                                callback.callback());

  callback.WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess, callback.status());
  EXPECT_EQ(0u, callback.value()->size());
}

TEST_F(FidoDeviceAuthenticatorTest, TestReadInvalidLargeBlob) {
  authenticator_state_->large_blob[0] += 1;
  ReadCallback callback;
  authenticator_->ReadLargeBlob({kDummyKey}, base::nullopt,
                                callback.callback());

  callback.WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrIntegrityFailure,
            callback.status());
  EXPECT_FALSE(callback.value());
}

// Test reading and writing a blob that fits in a single fragment.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteSmallBlob) {
  std::vector<uint8_t> small_blob = fido_parsing_utils::Materialize(kSmallBlob);
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(small_blob, {kDummyKey}, base::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, write_callback.value());

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey}, base::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, read_callback.status());
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(1u, large_blob_array->size());
  EXPECT_EQ(kDummyKey, large_blob_array->at(0).first);
  EXPECT_EQ(small_blob, large_blob_array->at(0).second);
}

// Test reading and writing a blob that must fit in multiple fragments.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlob) {
  std::vector<uint8_t> large_blob;
  large_blob.reserve(2048);
  for (size_t i = 0; i < large_blob.capacity(); ++i) {
    large_blob.emplace_back(i % 0xFF);
  }

  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(large_blob, {kDummyKey}, base::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, write_callback.value());

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey}, base::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, read_callback.status());
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(1u, large_blob_array->size());
  EXPECT_EQ(kDummyKey, large_blob_array->at(0).first);
  EXPECT_EQ(large_blob, large_blob_array->at(0).second);
}

}  // namespace

}  // namespace device
