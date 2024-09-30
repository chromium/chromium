// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/base/provision_fetcher.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

#define EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(a, b)                   \
  do {                                                              \
    if (!MediaDrmBridge::IsKeySystemSupported(b)) {                 \
      VLOG(0) << "Key System " << b << " not supported on device."; \
      EXPECT_FALSE(a);                                              \
    } else {                                                        \
      EXPECT_TRUE(a);                                               \
    }                                                               \
  } while (0)

#define EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(a, b, error)  \
  do {                                                              \
    if (!MediaDrmBridge::IsKeySystemSupported(b)) {                 \
      VLOG(0) << "Key System " << b << " not supported on device."; \
      EXPECT_EQ(a.code(), error);                                   \
    } else {                                                        \
      EXPECT_TRUE(a.has_value());                                   \
    }                                                               \
  } while (0)

const char kAudioMp4[] = "audio/mp4";
const char kVideoMp4[] = "video/mp4";
const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kInvalidKeySystem[] = "invalid.keysystem";
const MediaDrmBridge::SecurityLevel kDefault =
    MediaDrmBridge::SECURITY_LEVEL_DEFAULT;
const MediaDrmBridge::SecurityLevel kL1 = MediaDrmBridge::SECURITY_LEVEL_1;
const MediaDrmBridge::SecurityLevel kL3 = MediaDrmBridge::SECURITY_LEVEL_3;
const char kTestOrigin[] = "http://www.example.com";
const char kEmptyOrigin[] = "";

// Helper functions to avoid typing "MediaDrmBridge::" in tests.

static bool IsKeySystemSupportedWithType(
    const std::string& key_system,
    const std::string& container_mime_type) {
  return MediaDrmBridge::IsKeySystemSupportedWithType(key_system,
                                                      container_mime_type);
}

namespace {

// This class is simply a wrapper that passes on calls to Retrieve() to another
// implementation that is provided to the constructor. This is created as
// MediaDrmBridge::CreateWithoutSessionSupport() requires the creation of a new
// ProvisionFetcher each time it needs to retrieve a license from the license
// server.
class ProvisionFetcherWrapper : public ProvisionFetcher {
 public:
  explicit ProvisionFetcherWrapper(ProvisionFetcher* provision_fetcher)
      : provision_fetcher_(provision_fetcher) {}

  // ProvisionFetcher implementation.
  void Retrieve(const GURL& default_url,
                const std::string& request_data,
                ResponseCB response_cb) override {
    provision_fetcher_->Retrieve(default_url, request_data,
                                 std::move(response_cb));
  }

 private:
  raw_ptr<ProvisionFetcher> provision_fetcher_;
};

}  // namespace

class MediaDrmBridgeTest : public ProvisionFetcher, public testing::Test {
 public:
  MediaDrmBridgeTest() {}

  void CreateWithoutSessionSupport(
      const std::string& key_system,
      const std::string& origin_id,
      MediaDrmBridge::SecurityLevel security_level) {
    media_drm_bridge_ = MediaDrmBridge::CreateWithoutSessionSupport(
        key_system, origin_id, security_level, "Test",
        base::BindRepeating(&MediaDrmBridgeTest::CreateProvisionFetcher,
                            base::Unretained(this)));
  }

  void CreateWithoutSessionSupportWithNullCallback(
      const std::string& key_system,
      const std::string& origin_id,
      MediaDrmBridge::SecurityLevel security_level) {
    media_drm_bridge_ = MediaDrmBridge::CreateWithoutSessionSupport(
        key_system, origin_id, security_level, "Test", base::NullCallback());
  }

  // ProvisionFetcher implementation. Done as a mock method so we can properly
  // check if |media_drm_bridge_| invokes it or not.
  MOCK_METHOD(void,
              Retrieve,
              (const GURL& default_url,
               const std::string& request_data,
               ResponseCB response_cb));

  void Provision() {
    media_drm_bridge_->Provision(base::BindOnce(
        &MediaDrmBridgeTest::ProvisioningDone, base::Unretained(this)));
  }

  void Unprovision() { media_drm_bridge_->Unprovision(); }

  // MediaDrmBridge::Provision() requires a callback that is called when
  // provisioning completes and indicates if it succeeds or not.
  MOCK_METHOD(void, ProvisioningDone, (bool));

  // Called whenever Provision() is run to create a ProvisionFetcher (which if
  // needed will be `this`).
  MOCK_METHOD(std::unique_ptr<ProvisionFetcher>, CreateProvisionFetcher, ());

 protected:
  MediaDrmBridge::CdmCreationResult media_drm_bridge_ =
      CreateCdmTypedStatus::Codes::kUnknownError;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MediaDrmBridgeTest, IsKeySystemSupported_Widevine) {
  // TODO(xhwang): Enable when b/13564917 is fixed.
  // EXPECT_TRUE_IF_AVAILABLE(
  //     IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioMp4));
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoMp4),
      kWidevineKeySystem);

  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kAudioWebM),
      kWidevineKeySystem);
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      IsKeySystemSupportedWithType(kWidevineKeySystem, kVideoWebM),
      kWidevineKeySystem);

  EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, "unknown"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, "video/avi"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kWidevineKeySystem, "audio/mp3"));
}

TEST_F(MediaDrmBridgeTest, IsKeySystemSupported_ExternalClearKey) {
  // Testing that 'kExternalClearKeyForTesting' is disabled by default.
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting));
  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});
  EXPECT_TRUE(base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting));
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      IsKeySystemSupportedWithType(kExternalClearKeyKeySystem, kVideoMp4),
      kExternalClearKeyKeySystem);
}

// Invalid key system is NOT supported regardless whether MediaDrm is available.
TEST_F(MediaDrmBridgeTest, IsKeySystemSupported_InvalidKeySystem) {
  EXPECT_FALSE(MediaDrmBridge::IsKeySystemSupported(kInvalidKeySystem));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kAudioMp4));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kVideoMp4));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kAudioWebM));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, kVideoWebM));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, "unknown"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, "video/avi"));
  EXPECT_FALSE(IsKeySystemSupportedWithType(kInvalidKeySystem, "audio/mp3"));
}

TEST_F(MediaDrmBridgeTest, CreateWithoutSessionSupport_Widevine) {
  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kDefault);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kWidevineKeySystem,
      CreateCdmTypedStatus::Codes::kUnsupportedKeySystem);
}

TEST_F(MediaDrmBridgeTest, CreateWithoutSessionSupport_ExternalClearKey) {
  CreateWithoutSessionSupport(kExternalClearKeyKeySystem, kTestOrigin,
                              kDefault);
  EXPECT_EQ(media_drm_bridge_.code(),
            CreateCdmTypedStatus::Codes::kUnsupportedKeySystem);

  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});
  CreateWithoutSessionSupport(kExternalClearKeyKeySystem, kTestOrigin,
                              kDefault);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kExternalClearKeyKeySystem,
      CreateCdmTypedStatus::Codes::kUnsupportedKeySystem);
}

// Invalid key system is NOT supported regardless whether MediaDrm is available.
TEST_F(MediaDrmBridgeTest, CreateWithoutSessionSupport_InvalidKeySystem) {
  CreateWithoutSessionSupport(kInvalidKeySystem, kTestOrigin, kDefault);
  EXPECT_FALSE(media_drm_bridge_.has_value());
  EXPECT_EQ(media_drm_bridge_.code(),
            CreateCdmTypedStatus::Codes::kUnsupportedKeySystem);
}

TEST_F(MediaDrmBridgeTest, CreateWithSecurityLevel_Widevine) {
  // We test "L3" fully. But for "L1" we don't check the result as it depends on
  // whether the test device supports "L1".
  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL3);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kWidevineKeySystem,
      CreateCdmTypedStatus::Codes::kUnsupportedKeySystem);

  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL1);
}

TEST_F(MediaDrmBridgeTest, CreateWithSecurityLevel_ExternalClearKey) {
  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});

  // ExternalClearKey only initialized with 'kDefault' security level.
  CreateWithoutSessionSupport(kExternalClearKeyKeySystem, kTestOrigin,
                              kDefault);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kExternalClearKeyKeySystem,
      CreateCdmTypedStatus::Codes::kUnsupportedKeySystem);
}

TEST_F(MediaDrmBridgeTest, Provision_Widevine) {
  // Only test this if Widevine is supported. Otherwise
  // CreateWithoutSessionSupport() will return null and it can't be tested.
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  // Calling Provision() later should trigger a provisioning request. As we
  // can't pass the request to a license server,
  // MockProvisionFetcher::Retrieve() simply drops the request and never
  // responds. As a result, there should be a call to Retrieve() but not to
  // ProvisioningDone() (CB passed to Provision()) as the provisioning never
  // completes.
  EXPECT_CALL(*this, CreateProvisionFetcher())
      .WillOnce(
          Return(ByMove(std::make_unique<ProvisionFetcherWrapper>(this))));
  EXPECT_CALL(*this, Retrieve(_, _, _));
  EXPECT_CALL(*this, ProvisioningDone(_)).Times(0);

  // Create MediaDrmBridge. We only test "L3" as "L1" depends on whether the
  // test device supports it or not.
  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL3);
  EXPECT_TRUE(media_drm_bridge_.has_value());
  Provision();

  // Provisioning is executed asynchronously.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaDrmBridgeTest, Provision_Widevine_NoOrigin) {
  // Only test this if Widevine is supported. Otherwise
  // CreateWithoutSessionSupport() will return null and it can't be tested.
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  // Calling Provision() later should fail as the origin is not provided (or
  // origin isolated storage is not available). No provisioning request should
  // be attempted.
  EXPECT_CALL(*this, CreateProvisionFetcher()).Times(0);
  EXPECT_CALL(*this, Retrieve(_, _, _)).Times(0);
  EXPECT_CALL(*this, ProvisioningDone(false));

  // Create MediaDrmBridge. We only test "L3" as "L1" depends on whether the
  // test device supports it or not.
  CreateWithoutSessionSupport(kWidevineKeySystem, kEmptyOrigin, kL3);
  EXPECT_TRUE(media_drm_bridge_.has_value());
  Provision();

  // Provisioning is executed asynchronously.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaDrmBridgeTest, Unprovision_Widevine) {
  // Only test this if Widevine is supported. Otherwise
  // CreateWithoutSessionSupport() will return null and it can't be tested.
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  // Ensure Unprovision doesn't try to provision.
  EXPECT_CALL(*this, CreateProvisionFetcher()).Times(0);

  // Create MediaDrmBridge. We only test "L3" as "L1" depends on whether the
  // test device supports it or not.
  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL3);
  EXPECT_TRUE(media_drm_bridge_.has_value());
  Unprovision();
}

TEST_F(MediaDrmBridgeTest, GetStatusForPolicy_FeatureFlagDisabled) {
  // Only test this if Widevine is supported. Otherwise
  // CreateWithoutSessionSupport() will return null and it can't be
  // tested.
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  scoped_feature_list_.InitWithFeatureState(media::kMediaDrmGetStatusForPolicy,
                                            false);

  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL3);
  EXPECT_TRUE(media_drm_bridge_.has_value());

  CdmKeyInformation::KeyStatus key_status;
  CdmPromise::Exception exception;

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersionNone,
      std::make_unique<MockCdmKeyStatusPromise>(
          /*expect_success=*/false, &key_status, &exception));

  EXPECT_EQ(exception, CdmPromise::Exception::NOT_SUPPORTED_ERROR);
}

TEST_F(MediaDrmBridgeTest, GetStatusForPolicy_ExternalClearKey) {
  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});

  // TODO(b/263310318): Remove test skip when clear key is fixed and we call
  // into MediaDrm for Android ClearKey instead of using AesDecryptor.
  if (!MediaDrmBridge::IsKeySystemSupported(kExternalClearKeyKeySystem)) {
    GTEST_SKIP() << "ClearKey not supported on device.";
  }

  // ExternalClearKey only initialized with 'kDefault' security level.
  CreateWithoutSessionSupport(kExternalClearKeyKeySystem, kTestOrigin,
                              kDefault);
  EXPECT_TRUE(media_drm_bridge_.has_value());

  CdmKeyInformation::KeyStatus key_status;

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersionNone, std::make_unique<MockCdmKeyStatusPromise>(
                                         /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::USABLE, key_status);

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion1_0, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion2_3, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);
}

TEST_F(MediaDrmBridgeTest, GetStatusForPolicyL3_Widevine) {
  // Only test this if Widevine is supported. Otherwise
  // CreateWithoutSessionSupport() will return null and it can't be
  // tested.
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL3);
  EXPECT_TRUE(media_drm_bridge_.has_value());

  CdmKeyInformation::KeyStatus key_status;

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersionNone, std::make_unique<MockCdmKeyStatusPromise>(
                                         /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::USABLE, key_status);

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion1_0, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);

  media_drm_bridge_->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion2_3, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);
}

}  // namespace media
