// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

namespace {

using WriteCallback =
    device::test::ValueCallbackReceiver<CtapDeviceResponseCode>;
using ReadCallback = device::test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    absl::optional<std::vector<std::pair<LargeBlobKey, LargeBlob>>>>;
using PinCallback = device::test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    absl::optional<pin::TokenResponse>>;
using GarbageCollectionCallback =
    device::test::ValueCallbackReceiver<CtapDeviceResponseCode>;
using TouchCallback = device::test::TestCallbackReceiver<>;

constexpr LargeBlobKey kDummyKey1 = {{0x01}};
constexpr LargeBlobKey kDummyKey2 = {{0x02}};
// The actual values for the "original size" that these blobs are supposed to
// inflate to are not important here.
const LargeBlob kSmallBlob1({'r', 'o', 's', 'a'}, 42);
const LargeBlob kSmallBlob2({'l', 'u', 'm', 'a'}, 9000);
const LargeBlob kSmallBlob3({'s', 't', 'a', 'r'}, 99);
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
    config.credential_management_support = true;
    config.return_err_no_credentials_on_empty_rp_enumeration = true;
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

  cbor::Value::ArrayValue GetLargeBlobArray() {
    LargeBlobArrayReader reader;
    reader.Append(virtual_device_->mutable_state()->large_blob);
    return *reader.Materialize();
  }

  // GetPINToken configures a PIN and returns a PIN token. This means all
  // further large blob operations will require the token.
  pin::TokenResponse GetPINToken() {
    virtual_device_->SetPin(kPin);
    PinCallback pin_callback;
    authenticator_->GetPINToken(kPin, {pin::Permissions::kLargeBlobWrite},
                                /*rp_id=*/absl::nullopt,
                                pin_callback.callback());
    pin_callback.WaitForCallback();
    DCHECK_EQ(pin_callback.status(), CtapDeviceResponseCode::kSuccess);
    return *pin_callback.value();
  }

  scoped_refptr<VirtualFidoDevice::State> authenticator_state_;
  std::unique_ptr<FidoDeviceAuthenticator> authenticator_;
  raw_ptr<VirtualCtap2Device> virtual_device_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FidoDeviceAuthenticatorTest, TestReadEmptyLargeBlob) {
  ReadCallback callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, absl::nullopt,
                                callback.callback());

  callback.WaitForCallback();
  EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_EQ(callback.value()->size(), 0u);
}

TEST_F(FidoDeviceAuthenticatorTest, TestReadInvalidLargeBlob) {
  authenticator_state_->large_blob[0] += 1;
  ReadCallback callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, absl::nullopt,
                                callback.callback());

  callback.WaitForCallback();
  EXPECT_EQ(callback.status(),
            CtapDeviceResponseCode::kCtap2ErrIntegrityFailure);
  EXPECT_FALSE(callback.value());
}

// Test reading and writing a blob that fits in a single fragment.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteSmallBlob) {
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, absl::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(large_blob_array->at(0).first, kDummyKey1);
  EXPECT_EQ(large_blob_array->at(0).second, kSmallBlob1);
}

// Tests that attempting to write a large blob overwrites the entire array if it
// is corrupted.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteInvalidLargeBlob) {
  authenticator_state_->large_blob[0] += 1;
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback.callback());
  write_callback.WaitForCallback();
  EXPECT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, absl::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(large_blob_array->at(0).first, kDummyKey1);
  EXPECT_EQ(large_blob_array->at(0).second, kSmallBlob1);
}

// Regression test for crbug.com/1405288.
TEST_F(FidoDeviceAuthenticatorTest,
       TestWriteBlobDoesNotOverwriteNonStructuredData) {
  virtual_device_->mutable_state()->InjectOpaqueLargeBlob(
      cbor::Value("comet observatory"));

  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  EXPECT_EQ(large_blob_array[0].GetString(), "comet observatory");
  EXPECT_TRUE(LargeBlobData::Parse(large_blob_array[1]));
}

// Test reading and writing a blob that must fit in multiple fragments.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlob) {
  std::vector<uint8_t> large_blob_contents;
  large_blob_contents.reserve(2048);
  for (size_t i = 0; i < large_blob_contents.capacity(); ++i) {
    large_blob_contents.emplace_back(i % 0xFF);
  }
  LargeBlob large_blob(std::move(large_blob_contents), 9999);

  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(large_blob, {kDummyKey1}, absl::nullopt,
                                 write_callback.callback());

  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, absl::nullopt,
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
  pin::TokenResponse pin_token = GetPINToken();
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, pin_token,
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
  EXPECT_EQ(large_blob_array->at(0).second, kSmallBlob1);
}

// Test updating a large blob in an array with multiple entries corresponding to
// other keys.
TEST_F(FidoDeviceAuthenticatorTest, TestUpdateLargeBlob) {
  WriteCallback write_callback1;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback1.callback());
  write_callback1.WaitForCallback();
  ASSERT_EQ(write_callback1.value(), CtapDeviceResponseCode::kSuccess);

  WriteCallback write_callback2;
  authenticator_->WriteLargeBlob(kSmallBlob2, {kDummyKey2}, absl::nullopt,
                                 write_callback2.callback());
  write_callback2.WaitForCallback();
  ASSERT_EQ(write_callback2.value(), CtapDeviceResponseCode::kSuccess);

  // Update the first entry.
  WriteCallback write_callback3;
  authenticator_->WriteLargeBlob(kSmallBlob3, {kDummyKey1}, absl::nullopt,
                                 write_callback3.callback());
  write_callback3.WaitForCallback();
  ASSERT_EQ(write_callback3.value(), CtapDeviceResponseCode::kSuccess);

  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1, kDummyKey2}, absl::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  EXPECT_THAT(*large_blob_array, testing::UnorderedElementsAre(
                                     std::make_pair(kDummyKey1, kSmallBlob3),
                                     std::make_pair(kDummyKey2, kSmallBlob2)));
}

// Test attempting to write a large blob with a serialized size larger than the
// maximum. Chrome should not attempt writing the blob in this case.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlobTooLarge) {
  // First write a valid blob to make sure it isn't overwritten.
  WriteCallback write_callback1;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback1.callback());
  write_callback1.WaitForCallback();
  ASSERT_EQ(write_callback1.value(), CtapDeviceResponseCode::kSuccess);

  // Then, attempt writing a blob that is too large.
  std::vector<uint8_t> large_blob_contents;
  large_blob_contents.reserve(kLargeBlobStorageSize + 1);
  for (size_t i = 0; i < large_blob_contents.capacity(); ++i) {
    large_blob_contents.emplace_back(i % 0xFF);
  }
  LargeBlob large_blob(std::move(large_blob_contents), 9999);
  WriteCallback write_callback2;
  authenticator_->WriteLargeBlob(large_blob, {kDummyKey1}, absl::nullopt,
                                 write_callback2.callback());
  write_callback2.WaitForCallback();
  ASSERT_EQ(write_callback2.value(),
            CtapDeviceResponseCode::kCtap2ErrRequestTooLarge);

  // Make sure the first blob was not overwritten.
  ReadCallback read_callback;
  authenticator_->ReadLargeBlob({kDummyKey1}, absl::nullopt,
                                read_callback.callback());
  read_callback.WaitForCallback();
  ASSERT_EQ(read_callback.status(), CtapDeviceResponseCode::kSuccess);
  auto large_blob_array = read_callback.value();
  ASSERT_TRUE(large_blob_array);
  ASSERT_EQ(large_blob_array->size(), 1u);
  EXPECT_EQ(kDummyKey1, large_blob_array->at(0).first);
  EXPECT_EQ(kSmallBlob1, large_blob_array->at(0).second);
}

// Tests garbage collecting a large blob.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlob) {
  // Write a large blob corresponding to a key.
  std::vector<uint8_t> credential_id = std::vector<uint8_t>{1, 2, 3, 4};
  virtual_device_->mutable_state()->InjectResidentKey(
      credential_id, "galaxy.example.com", std::vector<uint8_t>{5, 6, 7, 8},
      absl::nullopt, absl::nullopt);
  virtual_device_->mutable_state()
      ->registrations.at(credential_id)
      .large_blob_key = kDummyKey1;
  WriteCallback write_callback1;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback1.callback());
  write_callback1.WaitForCallback();
  ASSERT_EQ(write_callback1.value(), CtapDeviceResponseCode::kSuccess);

  // Write an orphaned large blob.
  WriteCallback write_callback2;
  authenticator_->WriteLargeBlob(kSmallBlob2, {kDummyKey2}, absl::nullopt,
                                 write_callback2.callback());
  write_callback2.WaitForCallback();
  ASSERT_EQ(write_callback2.value(), CtapDeviceResponseCode::kSuccess);

  // Write an opaque large blob.
  virtual_device_->mutable_state()->InjectOpaqueLargeBlob(
      cbor::Value("comet observatory"));

  // At this point, there should be three blobs stored.
  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 3u);
  ASSERT_TRUE(
      LargeBlobData::Parse(large_blob_array.at(0))->Decrypt(kDummyKey1));
  ASSERT_TRUE(
      LargeBlobData::Parse(large_blob_array.at(1))->Decrypt(kDummyKey2));
  ASSERT_EQ(large_blob_array.at(2).GetString(), "comet observatory");

  // Perform garbage collection.
  GarbageCollectionCallback garbage_collection_callback;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), garbage_collection_callback.callback());
  garbage_collection_callback.WaitForCallback();
  EXPECT_EQ(garbage_collection_callback.value(),
            CtapDeviceResponseCode::kSuccess);

  // The second blob, which was orphaned, should have been deleted.
  large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 2u);
  EXPECT_TRUE(
      LargeBlobData::Parse(large_blob_array.at(0))->Decrypt(kDummyKey1));
  EXPECT_EQ(large_blob_array.at(1).GetString(), "comet observatory");
}

// Tests garbage collecting a large blob when no changes are needed.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlobNoChanges) {
  // Write a large blob corresponding to a key.
  std::vector<uint8_t> credential_id = std::vector<uint8_t>{1, 2, 3, 4};
  virtual_device_->mutable_state()->InjectResidentKey(
      credential_id, "galaxy.example.com", std::vector<uint8_t>{5, 6, 7, 8},
      absl::nullopt, absl::nullopt);
  virtual_device_->mutable_state()
      ->registrations.at(credential_id)
      .large_blob_key = kDummyKey1;
  WriteCallback write_callback1;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback1.callback());
  write_callback1.WaitForCallback();
  ASSERT_EQ(write_callback1.value(), CtapDeviceResponseCode::kSuccess);

  // Perform garbage collection.
  GarbageCollectionCallback gabarge_collection_callback;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), gabarge_collection_callback.callback());
  gabarge_collection_callback.WaitForCallback();
  EXPECT_EQ(gabarge_collection_callback.value(),
            CtapDeviceResponseCode::kSuccess);

  // The blob should still be there.
  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  EXPECT_TRUE(LargeBlobData::Parse(large_blob_array[0])->Decrypt(kDummyKey1));
}

// Tests that attempting to garbage collecting an invalid large blob replaces it
// with a new one.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlobInvalid) {
  std::vector<uint8_t> empty_large_blob = authenticator_state_->large_blob;

  // Write an invalid large blob.
  authenticator_state_->large_blob[0] += 1;

  // Perform garbage collection.
  GarbageCollectionCallback gabarge_collection_callback;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), gabarge_collection_callback.callback());
  gabarge_collection_callback.WaitForCallback();
  EXPECT_EQ(gabarge_collection_callback.value(),
            CtapDeviceResponseCode::kSuccess);

  // The blob should now be valid again.
  EXPECT_EQ(authenticator_state_->large_blob, empty_large_blob);
}

// Tests garbage collecting a large blob when there are no credentials.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlobNoCredentials) {
  // Write an orphaned large blob.
  WriteCallback write_callback;
  authenticator_->WriteLargeBlob(kSmallBlob1, {kDummyKey1}, absl::nullopt,
                                 write_callback.callback());
  write_callback.WaitForCallback();
  ASSERT_EQ(write_callback.value(), CtapDeviceResponseCode::kSuccess);

  // At this point, there should be a single blob stored.
  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 1u);
  ASSERT_TRUE(
      LargeBlobData::Parse(large_blob_array.at(0))->Decrypt(kDummyKey1));

  // Perform garbage collection.
  GarbageCollectionCallback garbage_collection_callback;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), garbage_collection_callback.callback());
  garbage_collection_callback.WaitForCallback();
  EXPECT_EQ(garbage_collection_callback.value(),
            CtapDeviceResponseCode::kSuccess);

  // The large blob array should now be empty.
  large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 0u);
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
