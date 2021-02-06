// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using WriteCallback =
    device::test::ValueCallbackReceiver<CtapDeviceResponseCode>;
using ReadCallback = device::test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    base::Optional<std::vector<std::pair<LargeBlobKey, std::vector<uint8_t>>>>>;
using PinCallback = device::test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    base::Optional<pin::TokenResponse>>;
using TouchCallback = device::test::TestCallbackReceiver<>;

constexpr LargeBlobKey kDummyKey1 = {{0x01}};
constexpr LargeBlobKey kDummyKey2 = {{0x02}};
constexpr std::array<uint8_t, 4> kSmallBlob1 = {'r', 'o', 's', 'a'};
constexpr std::array<uint8_t, 4> kSmallBlob2 = {'l', 'u', 'm', 'a'};
constexpr std::array<uint8_t, 4> kSmallBlob3 = {'s', 't', 'a', 'r'};
constexpr size_t kLargeBlobStorageSize = 4096;
constexpr char kPin[] = "1234";

class FidoDeviceAuthenticatorTest : public testing::Test {
 protected:
  void SetUp() override {
    VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.large_blob_support = true;
    config.resident_key_support = true;
    config.available_large_blob_storage = kLargeBlobStorageSize;
    config.pin_uv_auth_token_support = true;
    config.ctap2_versions = {Ctap2Version::kCtap2_1};
    SetUpAuthenticator(std::move(config));
  }

 protected:
  void SetUpAuthenticator(VirtualCtap2Device::Config config) {
    authenticator_state_ = base::MakeRefCounted<VirtualFidoDevice::State>();
    auto virtual_device =
        std::make_unique<VirtualCtap2Device>(authenticator_state_, config);
    virtual_device_ = virtual_device.get();
    authenticator_ =
        std::make_unique<FidoDeviceAuthenticator>(std::move(virtual_device));

    device::test::TestCallbackReceiver<> callback;
    authenticator_->InitializeAuthenticator(callback.callback());
    callback.WaitForCallback();
  }

  scoped_refptr<VirtualFidoDevice::State> authenticator_state_;
  std::unique_ptr<FidoDeviceAuthenticator> authenticator_;
  VirtualCtap2Device* virtual_device_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FidoDeviceAuthenticatorTest, TestReadEmptyLargeBlob) {
  ReadCallback callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, base::nullopt,
                                callback.callback());

  callback.WaitForCallback();
  EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(callback.value()->size(), 0u);
}

TEST_F(FidoDeviceAuthenticatorTest, TestReadInvalidLargeBlob) {
  authenticator_state_->large_blob[0] += 1;
  ReadCallback callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, base::nullopt,
                                callback.callback());

  callback.WaitForCallback();
  EXPECT_EQ(callback.status(),
            CtapDeviceResponseCode::kCtap2ErrIntegrityFailure);
  EXPECT_FALSE(callback.value());
}

// Test reading and writing a blob that fits in a single fragment.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteSmallBlob) {
  std::vector<uint8_t> small_blob =
      fido_parsing_utils::Materialize(kSmallBlob1);
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(small_blob, {kDummyKey1}, base::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, base::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(large_blob_array->at(0).first, kDummyKey1);
  EXPECT_EQ(large_blob_array->at(0).second, small_blob);
}

// Test reading and writing a blob that must fit in multiple fragments.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlob) {
  std::vector<uint8_t> large_blob;
  large_blob.reserve(2048);
  for (size_t i = 0; i < large_blob.capacity(); ++i) {
    large_blob.emplace_back(i % 0xFF);
  }

  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(large_blob, {kDummyKey1}, base::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, base::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(large_blob_array->at(0).first, kDummyKey1);
  EXPECT_EQ(large_blob_array->at(0).second, large_blob);
}

// Test reading and writing a blob using a PinUvAuthToken.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteSmallBlobWithToken) {
  virtual_device_->SetPin(kPin);
  PinCallback pin_callback;
  authenticator_->GetPINToken(kPin, {pin::Permissions::kLargeBlobWrite},
                              /*rp_id=*/base::nullopt, pin_callback.callback());
  pin_callback.WaitForCallback();
  ASSERT_EQ(pin_callback.status(), CtapDeviceResponseCode::kSuccess);
  pin::TokenResponse pin_token = *pin_callback.value();

  std::vector<uint8_t> small_blob =
      fido_parsing_utils::Materialize(kSmallBlob1);
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(small_blob, {kDummyKey1}, pin_token,
                                 write_callback.callback());
  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, pin_token,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(large_blob_array->at(0).first, kDummyKey1);
  EXPECT_EQ(large_blob_array->at(0).second, small_blob);
}

// Test updating a large blob in an array with multiple entries corresponding to
// other keys.
TEST_F(FidoDeviceAuthenticatorTest, TestUpdateLargeBlob) {
  WriteCallback write_callback1;
  authenticator_->WriteLargeBlob(fido_parsing_utils::Materialize(kSmallBlob1),
                                 {kDummyKey1}, base::nullopt,
                                 write_callback1.callback());
  write_callback1.WaitForCallback();
  ASSERT_EQ(write_callback1.value(), CtapDeviceResponseCode::kSuccess);

  WriteCallback write_callback2;
  std::vector<uint8_t> small_blob2 =
      fido_parsing_utils::Materialize(kSmallBlob2);
  authenticator_->WriteLargeBlob(small_blob2, {kDummyKey2}, base::nullopt,
                                 write_callback2.callback());
  write_callback2.WaitForCallback();
  ASSERT_EQ(write_callback2.value(), CtapDeviceResponseCode::kSuccess);

  // Update the first entry.
  WriteCallback write_callback3;
  std::vector<uint8_t> small_blob3 =
      fido_parsing_utils::Materialize(kSmallBlob3);
  authenticator_->WriteLargeBlob(small_blob3, {kDummyKey1}, base::nullopt,
                                 write_callback3.callback());
  write_callback3.WaitForCallback();
  ASSERT_EQ(write_callback3.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1, kDummyKey2}, base::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  EXPECT_THAT(*large_blob_array, testing::UnorderedElementsAre(
                                     std::make_pair(kDummyKey1, small_blob3),
                                     std::make_pair(kDummyKey2, small_blob2)));
}

// Test attempting to write a large blob with a serialized size larger than the
// maximum. Chrome should not attempt writing the blob in this case.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlobTooLarge) {
  // First write a valid blob to make sure it isn't overwritten.
  WriteCallback write_callback1;
  std::vector<uint8_t> small_blob =
      fido_parsing_utils::Materialize(kSmallBlob1);
  authenticator_->WriteLargeBlob(small_blob, {kDummyKey1}, base::nullopt,
                                 write_callback1.callback());
  write_callback1.WaitForCallback();
  ASSERT_EQ(write_callback1.value(), CtapDeviceResponseCode::kSuccess);

  // Then, attempt writing a blob that is too large.
  std::vector<uint8_t> large_blob;
  large_blob.reserve(kLargeBlobStorageSize + 1);
  for (size_t i = 0; i < large_blob.capacity(); ++i) {
    large_blob.emplace_back(i % 0xFF);
  }
  WriteCallback write_callback2;
  authenticator_->WriteLargeBlob(large_blob, {kDummyKey1}, base::nullopt,
                                 write_callback2.callback());
  write_callback2.WaitForCallback();
  ASSERT_EQ(write_callback2.value(),
            CtapDeviceResponseCode::kCtap2ErrRequestTooLarge);

  // Make sure the first blob was not overwritten.
  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, base::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(kDummyKey1, large_blob_array->at(0).first);
  EXPECT_EQ(small_blob, large_blob_array->at(0).second);
}

// Tests getting a touch.
TEST_F(FidoDeviceAuthenticatorTest, TestGetTouch) {
  for (Ctap2Version version :
       {Ctap2Version::kCtap2_0, Ctap2Version::kCtap2_1}) {
    SCOPED_TRACE(std::string("CTAP ") +
                 (version == Ctap2Version::kCtap2_0 ? "2.0" : "2.1"));
    VirtualCtap2Device::Config config;
    config.ctap2_versions = {version};
    SetUpAuthenticator(std::move(config));

    TouchCallback callback;
    bool touch_pressed = false;
    authenticator_state_->simulate_press_callback =
        base::BindLambdaForTesting([&](VirtualFidoDevice* device) {
          touch_pressed = true;
          return true;
        });
    authenticator_->GetTouch(callback.callback());
    callback.WaitForCallback();
    EXPECT_TRUE(touch_pressed);
  }
}

}  // namespace

}  // namespace device
