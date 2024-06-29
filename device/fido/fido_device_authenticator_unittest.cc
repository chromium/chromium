// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using GetAssertionFuture =
    base::test::TestFuture<GetAssertionStatus,
                           std::vector<AuthenticatorGetAssertionResponse>>;
using PinFuture = base::test::TestFuture<CtapDeviceResponseCode,
                                         std::optional<pin::TokenResponse>>;
using GarbageCollectionFuture = base::test::TestFuture<CtapDeviceResponseCode>;
using TouchFuture = base::test::TestFuture<void>;

const std::string kRpId = "galaxy.example.com";
const std::vector<uint8_t> kCredentialId1{1, 1, 1, 1};
const std::vector<uint8_t> kCredentialId2{2, 2, 2, 2};
const std::vector<uint8_t> kUserId1{7, 7, 7, 7};
const std::vector<uint8_t> kUserId2{8, 8, 8, 8};
// The actual values for the "original size" that these blobs are supposed to
// inflate to are not important here.
const std::vector<uint8_t> kSmallBlob1{'r', 'o', 's', 'a'};
const std::vector<uint8_t> kSmallBlob2{'l', 'u', 'm', 'a'};
const std::vector<uint8_t> kSmallBlob3{'s', 't', 'a', 'r'};
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
    CHECK(virtual_device->mutable_state()->InjectResidentKey(
        kCredentialId1, kRpId, kUserId1, "rosa", "Rosalina"));
    virtual_device->mutable_state()
        ->registrations.at(kCredentialId1)
        .large_blob_key = {{1}};

    virtual_device_ = virtual_device.get();
    authenticator_ =
        std::make_unique<FidoDeviceAuthenticator>(std::move(virtual_device));

    base::test::TestFuture<void> future;
    authenticator_->InitializeAuthenticator(future.GetCallback());
    EXPECT_TRUE(future.Wait());
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
    PinFuture pin_future;
    authenticator_->GetPINToken(
        kPin,
        {pin::Permissions::kLargeBlobWrite, pin::Permissions::kGetAssertion},
        kRpId, pin_future.GetCallback());
    EXPECT_TRUE(pin_future.Wait());
    DCHECK_EQ(std::get<0>(pin_future.Get()), CtapDeviceResponseCode::kSuccess);
    return *std::get<1>(pin_future.Get());
  }

  std::vector<AuthenticatorGetAssertionResponse> GetAssertion(
      CtapGetAssertionOptions options,
      std::vector<std::vector<uint8_t>> credential_ids) {
    CtapGetAssertionRequest request(kRpId, test_data::kClientDataJson);
    for (const std::vector<uint8_t>& credential_id : credential_ids) {
      request.allow_list.emplace_back(CredentialType::kPublicKey,
                                      credential_id);
    }
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    CHECK_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess)
        << " get assertion returned "
        << static_cast<unsigned>(std::get<0>(future.Get()));
    std::vector<AuthenticatorGetAssertionResponse> response =
        std::get<1>(future.Take());
    return response;
  }

  std::vector<AuthenticatorGetAssertionResponse> GetAssertionForRead(
      std::vector<std::vector<uint8_t>> credential_ids = {kCredentialId1}) {
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    return GetAssertion(std::move(options), std::move(credential_ids));
  }

  AuthenticatorGetAssertionResponse GetAssertionForWrite(
      base::span<const uint8_t> blob,
      std::vector<uint8_t> credential_id = kCredentialId1) {
    CtapGetAssertionOptions options;
    options.large_blob_write.emplace(blob.begin(), blob.end());
    std::vector<std::vector<uint8_t>> credential_ids;
    credential_ids.push_back(std::move(credential_id));
    std::vector<AuthenticatorGetAssertionResponse> responses =
        GetAssertion(std::move(options), std::move(credential_ids));
    CHECK_EQ(responses.size(), 1u);
    return std::move(responses.at(0));
  }

  scoped_refptr<VirtualFidoDevice::State> authenticator_state_;
  std::unique_ptr<FidoDeviceAuthenticator> authenticator_;
  raw_ptr<VirtualCtap2Device> virtual_device_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(FidoDeviceAuthenticatorTest, TestReadEmptyLargeBlob) {
  std::vector<AuthenticatorGetAssertionResponse> assertions =
      GetAssertionForRead();
  EXPECT_EQ(assertions.size(), 1u);
  EXPECT_FALSE(assertions.at(0).large_blob.has_value());
}

TEST_F(FidoDeviceAuthenticatorTest, TestReadInvalidLargeBlob) {
  authenticator_state_->large_blob[0] += 1;
  std::vector<AuthenticatorGetAssertionResponse> assertions =
      GetAssertionForRead();
  EXPECT_EQ(assertions.size(), 1u);
  EXPECT_FALSE(assertions.at(0).large_blob.has_value());
}

// Test reading and writing a blob that fits in a single fragment.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteSmallBlob) {
  AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
  EXPECT_TRUE(write.large_blob_written);
  std::vector<AuthenticatorGetAssertionResponse> read = GetAssertionForRead();
  EXPECT_EQ(read.at(0).large_blob, kSmallBlob1);
}

// Tests that attempting to write a large blob overwrites the entire array if it
// is corrupted.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteInvalidLargeBlob) {
  authenticator_state_->large_blob[0] += 1;
  AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
  EXPECT_TRUE(write.large_blob_written);
  std::vector<AuthenticatorGetAssertionResponse> read = GetAssertionForRead();
  EXPECT_EQ(read.at(0).large_blob, kSmallBlob1);
}

// Regression test for crbug.com/1405288.
TEST_F(FidoDeviceAuthenticatorTest,
       TestWriteBlobDoesNotOverwriteNonStructuredData) {
  virtual_device_->mutable_state()->InjectOpaqueLargeBlob(
      cbor::Value("comet observatory"));

  AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
  EXPECT_TRUE(write.large_blob_written);

  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  EXPECT_EQ(large_blob_array[0].GetString(), "comet observatory");
  EXPECT_TRUE(LargeBlobData::Parse(large_blob_array[1]));
}

// Test reading and writing a blob that must fit in multiple fragments.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlob) {
  std::array<uint8_t, 2048> large_blob;
  base::RandBytes(large_blob);
  AuthenticatorGetAssertionResponse write = GetAssertionForWrite(large_blob);
  EXPECT_TRUE(write.large_blob_written);

  std::vector<AuthenticatorGetAssertionResponse> read = GetAssertionForRead();
  EXPECT_EQ(base::span(*read.at(0).large_blob), large_blob);
}

// Test reading and writing a blob using a PinUvAuthToken.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteSmallBlobWithToken) {
  pin::TokenResponse pin_token = GetPINToken();
  {
    CtapGetAssertionOptions options;
    options.large_blob_write = kSmallBlob1;
    options.pin_uv_auth_token = pin_token;
    std::vector<AuthenticatorGetAssertionResponse> responses =
        GetAssertion(std::move(options), {kCredentialId1});
    EXPECT_EQ(responses.size(), 1u);
    EXPECT_TRUE(responses.at(0).large_blob_written);
  }

  {
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    options.pin_uv_auth_token = pin_token;
    std::vector<AuthenticatorGetAssertionResponse> responses =
        GetAssertion(std::move(options), {kCredentialId1});
    EXPECT_EQ(responses.size(), 1u);
    EXPECT_EQ(responses.at(0).large_blob, kSmallBlob1);
  }
}

// Test updating a large blob in an array with multiple entries corresponding to
// other keys.
TEST_F(FidoDeviceAuthenticatorTest, TestUpdateLargeBlob) {
  virtual_device_->mutable_state()->InjectResidentKey(kCredentialId2, kRpId,
                                                      kUserId2, "luma", "Luma");
  virtual_device_->mutable_state()
      ->registrations.at(kCredentialId2)
      .large_blob_key = {{2}};

  {
    AuthenticatorGetAssertionResponse write =
        GetAssertionForWrite(kSmallBlob1, kCredentialId1);
    EXPECT_TRUE(write.large_blob_written);
  }
  {
    AuthenticatorGetAssertionResponse write =
        GetAssertionForWrite(kSmallBlob2, kCredentialId2);
    EXPECT_TRUE(write.large_blob_written);
  }
  {
    // Update the first entry.
    AuthenticatorGetAssertionResponse write =
        GetAssertionForWrite(kSmallBlob3, kCredentialId1);
    EXPECT_TRUE(write.large_blob_written);
  }

  std::vector<AuthenticatorGetAssertionResponse> read =
      GetAssertionForRead(/*credential_ids=*/{});

  ASSERT_EQ(read.size(), 2u);
  auto first = base::ranges::find_if(read, [&](const auto& response) {
    return response.credential->id == kCredentialId1;
  });
  ASSERT_NE(first, read.end());
  EXPECT_EQ(first->large_blob, kSmallBlob3);

  auto second = base::ranges::find_if(read, [&](const auto& response) {
    return response.credential->id == kCredentialId2;
  });
  ASSERT_NE(second, read.end());
  EXPECT_EQ(second->large_blob, kSmallBlob2);
}

// Test attempting to write a large blob with a serialized size larger than the
// maximum. Chrome should not attempt writing the blob in this case.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlobTooLarge) {
  {
    // First write a valid blob to make sure it isn't overwritten.
    AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
    EXPECT_TRUE(write.large_blob_written);
  }

  // Then, attempt writing a blob that is too large. The blob will be
  // compressed, so fill it with random data so it doesn't shrink.
  std::vector<uint8_t> large_blob;
  large_blob.resize(kLargeBlobStorageSize * 2);
  base::RandBytes(large_blob);
  AuthenticatorGetAssertionResponse write =
      GetAssertionForWrite(std::move(large_blob));
  EXPECT_FALSE(write.large_blob_written);

  // Make sure the first blob was not overwritten.
  std::vector<AuthenticatorGetAssertionResponse> read =
      GetAssertionForRead(/*credential_ids=*/{});
  ASSERT_EQ(read.size(), 1u);
  EXPECT_EQ(read.at(0).large_blob, kSmallBlob1);
}

// Tests writing a large blob for a credential that does not have a large blob
// key set.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlobNoLargeBlobKey) {
  for (auto& registration : virtual_device_->mutable_state()->registrations) {
    registration.second.large_blob_key = std::nullopt;
  }
  AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
  EXPECT_FALSE(write.large_blob_written);
}

// Tests that a CTAP error returned while writing a large blob does not fail the
// entire assertion.
TEST_F(FidoDeviceAuthenticatorTest, TestWriteLargeBlobCtapError) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.large_blob_support = true;
  config.resident_key_support = true;
  config.available_large_blob_storage = kLargeBlobStorageSize;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {Ctap2Version::kCtap2_1};
  config.override_response_map[CtapRequestCommand::kAuthenticatorLargeBlobs] =
      CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
  SetUpAuthenticator(std::move(config));

  AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
  EXPECT_FALSE(write.large_blob_written);
}

// Tests garbage collecting a large blob.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlob) {
  {
    // First, write a large blob corresponding to a credential.
    AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
    EXPECT_TRUE(write.large_blob_written);
  }
  {
    // Write an orphaned large blob.
    virtual_device_->mutable_state()->InjectResidentKey(
        kCredentialId2, kRpId, kUserId2, "luma", "Luma");
    virtual_device_->mutable_state()
        ->registrations.at(kCredentialId2)
        .large_blob_key = {{2}};
    AuthenticatorGetAssertionResponse write =
        GetAssertionForWrite(kSmallBlob2, kCredentialId2);
    EXPECT_TRUE(write.large_blob_written);
    virtual_device_->mutable_state()->registrations.erase(kCredentialId2);
  }
  // Write an opaque large blob.
  virtual_device_->mutable_state()->InjectOpaqueLargeBlob(
      cbor::Value("comet observatory"));

  // At this point, there should be three blobs stored.
  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 3u);
  ASSERT_EQ(large_blob_array.at(2).GetString(), "comet observatory");

  // Perform garbage collection.
  GarbageCollectionFuture garbage_collection_future;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), garbage_collection_future.GetCallback());
  EXPECT_TRUE(garbage_collection_future.Wait());
  EXPECT_EQ(garbage_collection_future.Get(), CtapDeviceResponseCode::kSuccess);

  // The second blob, which was orphaned, should have been deleted.
  large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 2u);
  EXPECT_EQ(large_blob_array.at(1).GetString(), "comet observatory");

  // Make sure we did not delete the valid blob by reading it.
  std::vector<AuthenticatorGetAssertionResponse> read = GetAssertionForRead();
  ASSERT_EQ(read.size(), 1u);
  EXPECT_EQ(read.at(0).large_blob, kSmallBlob1);
}

// Tests garbage collecting a large blob when no changes are needed.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlobNoChanges) {
  {
    // First, write a large blob corresponding to a credential.
    AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
    EXPECT_TRUE(write.large_blob_written);
  }

  // Perform garbage collection.
  GarbageCollectionFuture gabarge_collection_future;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), gabarge_collection_future.GetCallback());
  EXPECT_TRUE(gabarge_collection_future.Wait());
  EXPECT_EQ(gabarge_collection_future.Get(), CtapDeviceResponseCode::kSuccess);

  // The blob should still be there.
  std::vector<AuthenticatorGetAssertionResponse> read = GetAssertionForRead();
  ASSERT_EQ(read.size(), 1u);
  EXPECT_EQ(read.at(0).large_blob, kSmallBlob1);
}

// Tests that attempting to garbage collect an invalid large blob replaces it
// with a new one.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlobInvalid) {
  std::vector<uint8_t> empty_large_blob = authenticator_state_->large_blob;

  // Write an invalid large blob.
  authenticator_state_->large_blob[0] += 1;

  // Perform garbage collection.
  GarbageCollectionFuture gabarge_collection_future;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), gabarge_collection_future.GetCallback());
  EXPECT_TRUE(gabarge_collection_future.Wait());
  EXPECT_EQ(gabarge_collection_future.Get(), CtapDeviceResponseCode::kSuccess);

  // The blob should now be valid again.
  EXPECT_EQ(authenticator_state_->large_blob, empty_large_blob);
}

// Tests garbage collecting a large blob when there are no credentials.
TEST_F(FidoDeviceAuthenticatorTest, TestGarbageCollectLargeBlobNoCredentials) {
  {
    // Write an orphaned large blob.
    AuthenticatorGetAssertionResponse write = GetAssertionForWrite(kSmallBlob1);
    EXPECT_TRUE(write.large_blob_written);
    virtual_device_->mutable_state()->registrations.clear();
  }

  // At this point, there should be a single blob stored.
  cbor::Value::ArrayValue large_blob_array = GetLargeBlobArray();
  ASSERT_EQ(large_blob_array.size(), 1u);

  // Perform garbage collection.
  GarbageCollectionFuture garbage_collection_future;
  authenticator_->GarbageCollectLargeBlob(
      GetPINToken(), garbage_collection_future.GetCallback());
  EXPECT_TRUE(garbage_collection_future.Wait());
  EXPECT_EQ(garbage_collection_future.Get(), CtapDeviceResponseCode::kSuccess);

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

    TouchFuture future;
    bool touch_pressed = false;
    authenticator_state_->simulate_press_callback =
        base::BindLambdaForTesting([&](VirtualFidoDevice* device) {
          touch_pressed = true;
          return true;
        });
    authenticator_->GetTouch(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_TRUE(touch_pressed);
  }
}

}  // namespace

}  // namespace device
