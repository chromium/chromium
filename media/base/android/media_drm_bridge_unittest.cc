// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/barrier_closure.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "media/base/android/media_drm_bridge_factory.h"
#include "media/base/cdm_config.h"
#include "media/base/media_drm_storage.h"
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

#define EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(a, b, err)    \
  do {                                                              \
    if (!MediaDrmBridge::IsKeySystemSupported(b)) {                 \
      VLOG(0) << "Key System " << b << " not supported on device."; \
      EXPECT_EQ(a.error(), err);                                    \
    } else {                                                        \
      EXPECT_TRUE(a.has_value());                                   \
    }                                                               \
  } while (0)

const char kVideoMp4[] = "video/mp4";
const char kVideoWebM[] = "video/webm";
const char kInvalidKeySystem[] = "invalid.keysystem";
const MediaDrmBridge::SecurityLevel kDefault =
    MediaDrmBridge::SECURITY_LEVEL_UNKNOWN;
const MediaDrmBridge::SecurityLevel kL1 =
    MediaDrmBridge::SECURITY_LEVEL_HW_SECURE_ALL;
const MediaDrmBridge::SecurityLevel kL3 =
    MediaDrmBridge::SECURITY_LEVEL_SW_SECURE_CRYPTO;
const char kTestOrigin[] = "http://www.example.com";
const char kEmptyOrigin[] = "";

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
    media_drm_bridge_.value()->Provision(base::BindOnce(
        &MediaDrmBridgeTest::ProvisioningDone, base::Unretained(this)));
  }

  void Unprovision() { media_drm_bridge_.value()->Unprovision(); }

  // MediaDrmBridge::Provision() requires a callback that is called when
  // provisioning completes and indicates if it succeeds or not.
  MOCK_METHOD(void, ProvisioningDone, (bool));

  // Called whenever Provision() is run to create a ProvisionFetcher (which if
  // needed will be `this`).
  MOCK_METHOD(std::unique_ptr<ProvisionFetcher>, CreateProvisionFetcher, ());

 protected:
  MediaDrmBridge::CdmCreationResult media_drm_bridge_ =
      base::unexpected(CreateCdmStatus::kUnknownError);

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MediaDrmBridgeTest, IsKeySystemSupported_Widevine) {
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem),
      kWidevineKeySystem);
}

TEST_F(MediaDrmBridgeTest, GetSupportedContainers_Widevine) {
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      MediaDrmBridge::GetSupportedContainers(kWidevineKeySystem)
          .contains(kVideoMp4),
      kWidevineKeySystem);
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      MediaDrmBridge::GetSupportedContainers(kWidevineKeySystem)
          .contains(kVideoWebM),
      kWidevineKeySystem);
}

TEST_F(MediaDrmBridgeTest, GetSupportedContainers_WidevineWithSecurityLevel) {
  // Querying with explicit SECURITY_LEVEL_SW_SECURE_CRYPTO should be true
  // if Widevine is available.
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      MediaDrmBridge::GetSupportedContainers(
          kWidevineKeySystem, MediaDrmBridge::SECURITY_LEVEL_SW_SECURE_CRYPTO)
          .contains(kVideoMp4),
      kWidevineKeySystem);

  // Querying with explicit SECURITY_LEVEL_HW_SECURE_ALL may or may not be true
  // depending on device support, but should not crash.
  MediaDrmBridge::GetSupportedContainers(
      kWidevineKeySystem, MediaDrmBridge::SECURITY_LEVEL_HW_SECURE_ALL);
}

TEST_F(MediaDrmBridgeTest, IsKeySystemSupported_ExternalClearKey) {
  // Testing that 'kExternalClearKeyForTesting' is disabled by default.
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting));
  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});
  EXPECT_TRUE(base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting));
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      MediaDrmBridge::IsKeySystemSupported(kExternalClearKeyKeySystem),
      kExternalClearKeyKeySystem);
}

TEST_F(MediaDrmBridgeTest, GetSupportedContainers_ExternalClearKey) {
  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});
  EXPECT_TRUE_IF_KEY_SYSTEM_AVAILABLE(
      MediaDrmBridge::GetSupportedContainers(kExternalClearKeyKeySystem)
          .contains(kVideoMp4),
      kExternalClearKeyKeySystem);
}

// Invalid key system is NOT supported regardless whether MediaDrm is available.
TEST_F(MediaDrmBridgeTest, IsKeySystemSupported_InvalidKeySystem) {
  EXPECT_FALSE(MediaDrmBridge::IsKeySystemSupported(kInvalidKeySystem));
  EXPECT_TRUE(
      MediaDrmBridge::GetSupportedContainers(kInvalidKeySystem).empty());
  EXPECT_TRUE(
      MediaDrmBridge::GetSupportedContainers(
          kInvalidKeySystem, MediaDrmBridge::SECURITY_LEVEL_SW_SECURE_CRYPTO)
          .empty());
  EXPECT_TRUE(
      MediaDrmBridge::GetSupportedContainers(
          kInvalidKeySystem, MediaDrmBridge::SECURITY_LEVEL_HW_SECURE_ALL)
          .empty());
}

TEST_F(MediaDrmBridgeTest, CreateWithoutSessionSupport_Widevine) {
  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kDefault);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kWidevineKeySystem,
      CreateCdmStatus::kUnsupportedKeySystem);
}

TEST_F(MediaDrmBridgeTest, CreateWithoutSessionSupport_ExternalClearKey) {
  CreateWithoutSessionSupport(kExternalClearKeyKeySystem, kTestOrigin,
                              kDefault);
  EXPECT_EQ(media_drm_bridge_.error(), CreateCdmStatus::kUnsupportedKeySystem);

  scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                        {});
  CreateWithoutSessionSupport(kExternalClearKeyKeySystem, kTestOrigin,
                              kDefault);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kExternalClearKeyKeySystem,
      CreateCdmStatus::kUnsupportedKeySystem);
}

// Invalid key system is NOT supported regardless whether MediaDrm is available.
TEST_F(MediaDrmBridgeTest, CreateWithoutSessionSupport_InvalidKeySystem) {
  CreateWithoutSessionSupport(kInvalidKeySystem, kTestOrigin, kDefault);
  EXPECT_FALSE(media_drm_bridge_.has_value());
  EXPECT_EQ(media_drm_bridge_.error(), CreateCdmStatus::kUnsupportedKeySystem);
}

TEST_F(MediaDrmBridgeTest, CreateWithSecurityLevel_Widevine) {
  // We test "L3" fully. But for "L1" we don't check the result as it depends on
  // whether the test device supports "L1".
  CreateWithoutSessionSupport(kWidevineKeySystem, kTestOrigin, kL3);
  EXPECT_VALUE_IF_KEY_SYSTEM_AVAILABLE_OR_ERROR(
      media_drm_bridge_, kWidevineKeySystem,
      CreateCdmStatus::kUnsupportedKeySystem);

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
      CreateCdmStatus::kUnsupportedKeySystem);
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

// Provisioning for MediaCrypto creation is serialized across all
// MediaDrmBridge instances in the process: while one instance provisions,
// others that need provisioning wait for it. Destroying the provisioning
// instance before its request completes must wake the waiting instances, and
// waiting instances that were destroyed in the meantime must be skipped.
TEST_F(MediaDrmBridgeTest, ProvisionAfterDestroyedWhileProvisioning_Widevine) {
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  // Each MediaDrmBridge uses a new origin ID, so creating its MediaCrypto
  // requires provisioning. Retrieve() drops the request, so provisioning never
  // completes. Only `bridge1` and `bridge3` should send a request.
  int retrieve_count = 0;
  EXPECT_CALL(*this, CreateProvisionFetcher())
      .Times(2)
      .WillRepeatedly([this]() -> std::unique_ptr<ProvisionFetcher> {
        return std::make_unique<ProvisionFetcherWrapper>(this);
      });
  EXPECT_CALL(*this, Retrieve(_, _, _)).Times(2).WillRepeatedly([&]() {
    ++retrieve_count;
  });

  auto create_with_media_crypto = [this]() {
    return MediaDrmBridge::CreateInternal(
        MediaDrmBridge::GetUUID(kWidevineKeySystem),
        base::UnguessableToken::Create().ToString(), kL3, "Test",
        /*requires_media_crypto=*/true,
        std::make_unique<MediaDrmStorageBridge>(),
        base::BindRepeating(&MediaDrmBridgeTest::CreateProvisionFetcher,
                            base::Unretained(this)),
        SessionMessageCB(), SessionClosedCB(), SessionKeysChangeCB(),
        SessionExpirationUpdateCB());
  };

  // `bridge1` starts provisioning.
  MediaDrmBridge::CdmCreationResult bridge1 = create_with_media_crypto();
  ASSERT_TRUE(bridge1.has_value());
  ASSERT_TRUE(base::test::RunUntil([&]() { return retrieve_count == 1; }));

  // `bridge2` and `bridge3` also need provisioning, so they wait for `bridge1`.
  MediaDrmBridge::CdmCreationResult bridge2 = create_with_media_crypto();
  ASSERT_TRUE(bridge2.has_value());
  MediaDrmBridge::CdmCreationResult bridge3 = create_with_media_crypto();
  ASSERT_TRUE(bridge3.has_value());

  // Destroy `bridge2` while it is waiting.
  bridge2 = base::unexpected(CreateCdmStatus::kUnknownError);

  // Destroy `bridge1` while its provisioning is pending. The destroyed
  // `bridge2` must be skipped, and `bridge3` must send its own provisioning
  // request.
  bridge1 = base::unexpected(CreateCdmStatus::kUnknownError);
  EXPECT_TRUE(base::test::RunUntil([&]() { return retrieve_count == 2; }));
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

  media_drm_bridge_.value()->GetStatusForPolicy(
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

  media_drm_bridge_.value()->GetStatusForPolicy(
      HdcpVersion::kHdcpVersionNone, std::make_unique<MockCdmKeyStatusPromise>(
                                         /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::USABLE, key_status);

  media_drm_bridge_.value()->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion1_0, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);

  media_drm_bridge_.value()->GetStatusForPolicy(
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

  media_drm_bridge_.value()->GetStatusForPolicy(
      HdcpVersion::kHdcpVersionNone, std::make_unique<MockCdmKeyStatusPromise>(
                                         /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::USABLE, key_status);

  media_drm_bridge_.value()->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion1_0, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);

  media_drm_bridge_.value()->GetStatusForPolicy(
      HdcpVersion::kHdcpVersion2_3, std::make_unique<MockCdmKeyStatusPromise>(
                                        /*expect_success=*/true, &key_status));
  EXPECT_EQ(CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED, key_status);
}

TEST_F(MediaDrmBridgeTest, MaybeParseCdmVersion) {
  // Widevine version string returned from MediaDrm pre Android 15.
  auto old_wv_version_string =
      MediaDrmBridge::MaybeParseCdmVersion("18.0.0@341113000");
  EXPECT_TRUE(old_wv_version_string.IsValid());
  EXPECT_EQ(old_wv_version_string.GetString(), "18.0.0");

  // Version string with build tag that contain alphabets. This is the behavior
  // from Android 15 onwards for Widevine.
  base::Version current_wv_version_string =
      MediaDrmBridge::MaybeParseCdmVersion("19.5.0@BV1A.250226.001");
  EXPECT_TRUE(current_wv_version_string.IsValid());
  EXPECT_EQ(current_wv_version_string.GetString(), "19.5.0");

  // Empty string. This is returned by key systems that have not implemented or
  // set the Version property. This is an invalid version, and is handled by
  // callers of MediaDrmBridge::GetVersion.
  base::Version empty_version = MediaDrmBridge::MaybeParseCdmVersion("");
  EXPECT_FALSE(empty_version.IsValid());

  // Invalid parsed string without an '@'. Verify that the parsing function
  // does not crash, and it returns an invalid version.
  base::Version invalid_version =
      MediaDrmBridge::MaybeParseCdmVersion("invalid_version");
  EXPECT_FALSE(invalid_version.IsValid());
}

namespace {

class FakeMediaDrmStorage : public MediaDrmStorage {
 public:
  explicit FakeMediaDrmStorage(InitCB* out_init_cb)
      : out_init_cb_(out_init_cb) {}
  void Initialize(InitCB init_cb) override {
    *out_init_cb_ = std::move(init_cb);
  }
  void OnProvisioned(ResultCB) override { NOTREACHED(); }
  void SavePersistentSession(const std::string&,
                             const SessionData&,
                             ResultCB) override {
    NOTREACHED();
  }
  void LoadPersistentSession(const std::string&,
                             LoadPersistentSessionCB) override {
    NOTREACHED();
  }
  void RemovePersistentSession(const std::string&, ResultCB) override {
    NOTREACHED();
  }
  base::WeakPtr<MediaDrmStorage> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<InitCB> out_init_cb_;
  base::WeakPtrFactory<FakeMediaDrmStorage> weak_factory_{this};
};

}  // namespace

class MediaDrmBridgeFactoryTest : public testing::Test {
 protected:
  static std::unique_ptr<MediaDrmBridgeFactory> CreateFactory(
      MediaDrmStorage::InitCB* out_init_cb) {
    return std::make_unique<MediaDrmBridgeFactory>(
        base::BindRepeating(
            []() -> std::unique_ptr<ProvisionFetcher> { NOTREACHED(); }),
        base::BindRepeating(
            [](MediaDrmStorage::InitCB* out_init_cb)
                -> std::unique_ptr<MediaDrmStorage> {
              return std::make_unique<FakeMediaDrmStorage>(out_init_cb);
            },
            out_init_cb));
  }

  static void TriggerMediaCryptoFailure(
      MediaDrmBridgeFactory* factory,
      std::optional<MediaDrmBridge::MediaDrmCreateError> error) {
    MediaDrmBridge* bridge = factory->media_drm_bridge_.get();
    ASSERT_TRUE(bridge);
    bridge->SetMediaCryptoReadyCB(
        base::BindOnce(&MediaDrmBridgeFactory::OnMediaCryptoReady,
                       factory->weak_factory_.GetWeakPtr()));
    JNIEnv* env = base::android::AttachCurrentThread();
    if (error.has_value()) {
      bridge->OnCreateError(env, static_cast<int32_t>(*error));
    }
    bridge->OnMediaCryptoReady(env, nullptr);
  }

  static bool HasBridge(MediaDrmBridgeFactory* factory) {
    return factory->media_drm_bridge_ != nullptr;
  }

  static MediaDrmBridge* GetBridge(MediaDrmBridgeFactory* factory) {
    return factory->media_drm_bridge_.get();
  }

  static scoped_refptr<MediaDrmBridge> CreateL3Bridge() {
    auto result = MediaDrmBridge::CreateInternal(
        MediaDrmBridge::GetUUID(kWidevineKeySystem), kEmptyOrigin, kL3, "Test",
        /*requires_media_crypto=*/false,
        std::make_unique<MediaDrmStorageBridge>(), base::NullCallback(),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing());
    EXPECT_TRUE(result.has_value());
    return result.has_value() ? std::move(result).value() : nullptr;
  }

  static CreateCdmStatus GetLastCreateErrorStatus(
      const MediaDrmBridge* bridge) {
    return bridge->GetLastCreateErrorStatus();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(MediaDrmBridgeFactoryTest, NotifyMediaCryptoReady_NoDoubleCallback) {
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  base::HistogramTester histogram_tester;
  scoped_refptr<MediaDrmBridge> bridge = CreateL3Bridge();
  ASSERT_TRUE(bridge);

  base::test::TestFuture<base::android::ScopedJavaGlobalRef<jobject>, bool>
      first_ready;
  bridge->SetMediaCryptoReadyCB(first_ready.GetCallback());
  JNIEnv* env = base::android::AttachCurrentThread();
  bridge->OnCreateError(
      env,
      static_cast<int32_t>(
          MediaDrmBridge::MediaDrmCreateError::FAILED_MEDIA_CRYPTO_SESSION));
  bridge->OnMediaCryptoReady(env, nullptr);
  EXPECT_TRUE(first_ready.Get<0>().is_null());
  EXPECT_FALSE(first_ready.Get<1>());
  EXPECT_EQ(GetLastCreateErrorStatus(bridge.get()),
            CreateCdmStatus::kAndroidFailedMediaCryptoSession);
  histogram_tester.ExpectUniqueSample("Media.EME.MediaCryptoAvailable", false,
                                      1);

  base::test::TestFuture<base::android::ScopedJavaGlobalRef<jobject>, bool>
      second_ready;
  bridge->SetMediaCryptoReadyCB(second_ready.GetCallback());
  EXPECT_TRUE(second_ready.IsReady());
  EXPECT_TRUE(second_ready.Get<0>().is_null());
  histogram_tester.ExpectTotalCount("Media.EME.MediaCryptoAvailable", 1);
}

TEST_F(MediaDrmBridgeFactoryTest, OnMediaCryptoReadySurfacesCreateErrors) {
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  CdmConfig config;
  config.key_system = kWidevineKeySystem;
  {
    MediaDrmStorage::InitCB init_cb;
    auto factory = CreateFactory(&init_cb);
    base::test::TestFuture<const scoped_refptr<ContentDecryptionModule>&,
                           CreateCdmStatus>
        future;
    factory->Create(config, base::DoNothing(), base::DoNothing(),
                    base::DoNothing(), base::DoNothing(), future.GetCallback());
    std::move(init_cb).Run(false, std::nullopt);
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(), CreateCdmStatus::kGetCdmOriginIdFailed);
  }

  const struct {
    std::optional<MediaDrmBridge::MediaDrmCreateError> error;
    CreateCdmStatus expected_status;
  } kCases[] = {
      {std::nullopt, CreateCdmStatus::kMediaCryptoNotAvailable},
      {MediaDrmBridge::MediaDrmCreateError::FAILED_SECURITY_ORIGIN,
       CreateCdmStatus::kAndroidFailedSecurityOrigin},
      {MediaDrmBridge::MediaDrmCreateError::FAILED_MEDIA_CRYPTO_SESSION,
       CreateCdmStatus::kAndroidFailedMediaCryptoSession},
      {MediaDrmBridge::MediaDrmCreateError::FAILED_TO_START_PROVISIONING,
       CreateCdmStatus::kAndroidFailedToStartProvisioning},
      {MediaDrmBridge::MediaDrmCreateError::FAILED_MEDIA_CRYPTO_CREATE,
       CreateCdmStatus::kAndroidFailedMediaCryptoCreate},
      {MediaDrmBridge::MediaDrmCreateError::UNSUPPORTED_MEDIACRYPTO_SCHEME,
       CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme},
  };
  for (const auto& tc : kCases) {
    base::HistogramTester histogram_tester;
    MediaDrmStorage::InitCB init_cb;
    auto factory = CreateFactory(&init_cb);
    base::test::TestFuture<const scoped_refptr<ContentDecryptionModule>&,
                           CreateCdmStatus>
        future;
    factory->Create(config, base::DoNothing(), base::DoNothing(),
                    base::DoNothing(), base::DoNothing(), future.GetCallback());
    TriggerMediaCryptoFailure(factory.get(), tc.error);
    EXPECT_FALSE(future.Get<0>());
    EXPECT_EQ(future.Get<1>(), tc.expected_status);
    EXPECT_FALSE(HasBridge(factory.get()));
    histogram_tester.ExpectUniqueSample("Media.EME.MediaCryptoAvailable", false,
                                        1);
  }
}

// Drives init_cb -> OnStorageInitialized(true) -> CompleteInitialization()
// with setOrigin() failing in Java. The create callback must not run
// synchronously from CompleteInitialization(); it must wait for Java's
// release() to notify a null MediaCrypto, and report the origin error once.
TEST_F(MediaDrmBridgeFactoryTest, CompleteInitializationFailureWaitsForJava) {
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  base::HistogramTester histogram_tester;
  CdmConfig config;
  config.key_system = kWidevineKeySystem;
  MediaDrmStorage::InitCB init_cb;
  auto factory = CreateFactory(&init_cb);
  base::test::TestFuture<const scoped_refptr<ContentDecryptionModule>&,
                         CreateCdmStatus>
      future;
  factory->Create(config, base::DoNothing(), base::DoNothing(),
                  base::DoNothing(), base::DoNothing(), future.GetCallback());
  ASSERT_TRUE(init_cb);
  ASSERT_TRUE(HasBridge(factory.get()));
  GetBridge(factory.get())->SetFailSetOriginForTesting(true);

  std::move(init_cb).Run(true, base::UnguessableToken::Create());
  EXPECT_FALSE(future.IsReady());

  EXPECT_FALSE(future.Get<0>());
  EXPECT_EQ(future.Get<1>(), CreateCdmStatus::kAndroidFailedSecurityOrigin);
  EXPECT_FALSE(HasBridge(factory.get()));
  histogram_tester.ExpectUniqueSample("Media.EME.MediaCryptoAvailable", false,
                                      1);
}

TEST_F(MediaDrmBridgeFactoryTest, AbortDuringPendingMediaCryptoReady) {
  if (!MediaDrmBridge::IsKeySystemSupported(kWidevineKeySystem)) {
    GTEST_SKIP() << "Widevine not supported on device.";
  }

  CdmConfig config;
  config.key_system = kWidevineKeySystem;
  base::test::TestFuture<const scoped_refptr<ContentDecryptionModule>&,
                         CreateCdmStatus>
      future;
  MediaDrmStorage::InitCB init_cb;
  {
    auto factory = CreateFactory(&init_cb);
    factory->Create(config, base::DoNothing(), base::DoNothing(),
                    base::DoNothing(), base::DoNothing(), future.GetCallback());
    TriggerMediaCryptoFailure(
        factory.get(),
        MediaDrmBridge::MediaDrmCreateError::FAILED_MEDIA_CRYPTO_SESSION);
  }
  if (init_cb) {
    std::move(init_cb).Run(false, std::nullopt);
  }
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(future.Get<0>());
  EXPECT_EQ(future.Get<1>(), CreateCdmStatus::kCdmCreationAborted);
}

TEST_F(MediaDrmBridgeTest,
       UpdateSession_ClearKey_DeliversListenerCallbacksOnTaskRunner) {
  StrictMock<MockCdmClient> client;
  auto creation_result = MediaDrmBridge::CreateInternal(
      base::ToVector(kClearKeyUuid), kEmptyOrigin, kDefault, "Test",
      /*requires_media_crypto=*/true, std::make_unique<MediaDrmStorageBridge>(),
      base::NullCallback(),
      base::BindRepeating(&MockCdmClient::OnSessionMessage,
                          base::Unretained(&client)),
      base::BindRepeating(&MockCdmClient::OnSessionClosed,
                          base::Unretained(&client)),
      base::BindRepeating(&MockCdmClient::OnSessionKeysChange,
                          base::Unretained(&client)),
      base::BindRepeating(&MockCdmClient::OnSessionExpirationUpdate,
                          base::Unretained(&client)));
  if (!creation_result.has_value() &&
      creation_result.error() == CreateCdmStatus::kUnsupportedKeySystem) {
    GTEST_SKIP() << "ClearKey not supported on device.";
  }
  ASSERT_TRUE(creation_result.has_value());
  scoped_refptr<MediaDrmBridge> bridge = std::move(creation_result).value();

  std::string session_id;
  base::RunLoop create_loop;
  EXPECT_CALL(client, OnSessionMessage(_, _, _)).WillOnce([&] {
    create_loop.Quit();
  });
  bridge->CreateSessionAndGenerateRequest(
      CdmSessionType::kTemporary, EmeInitDataType::WEBM,
      std::vector<uint8_t>(16, 0x01),
      std::make_unique<MockCdmSessionPromise>(/*expect_success=*/true,
                                              &session_id));
  create_loop.Run();
  ASSERT_FALSE(session_id.empty());

  // ClearKey's provideKeyResponse() fires onEvent, onExpirationUpdate, and
  // onKeyStatusChange. Because TaskEnvironment does not pump Android's Looper,
  // callbacks only arrive via PostToTaskRunner() after UpdateSession()
  // resolves.
  base::RunLoop update_loop;
  bool update_resolved = false;
  base::RepeatingClosure done =
      base::BarrierClosure(4, update_loop.QuitClosure());
  auto update_promise =
      std::make_unique<MockCdmPromise>(/*expect_success=*/true);
  ON_CALL(*update_promise, resolve()).WillByDefault([&] {
    update_resolved = true;
    done.Run();
  });
  auto expect_after_resolve = [&] {
    EXPECT_TRUE(update_resolved);
    done.Run();
  };
  EXPECT_CALL(client, OnSessionMessage(session_id, _, _))
      .WillOnce(expect_after_resolve);
  EXPECT_CALL(client, OnSessionKeysChangeCalled(session_id, true))
      .WillOnce(expect_after_resolve);
  EXPECT_CALL(client, OnSessionExpirationUpdate(session_id, _))
      .WillOnce(expect_after_resolve);

  constexpr std::string_view kClearKeyJwkSet =
      R"({"keys":[{"kty":"oct","kid":"AQEBAQEBAQEBAQEBAQEBAQ",)"
      R"("k":"AQEBAQEBAQEBAQEBAQEBAQ"}]})";
  bridge->UpdateSession(session_id, base::ToVector<uint8_t>(kClearKeyJwkSet),
                        std::move(update_promise));
  update_loop.Run();
  EXPECT_FALSE(client.keys_info().empty());

  base::RunLoop close_loop;
  auto close_promise =
      std::make_unique<MockCdmPromise>(/*expect_success=*/true);
  ON_CALL(*close_promise, resolve()).WillByDefault([&] { close_loop.Quit(); });
  EXPECT_CALL(client, OnSessionClosed(session_id, _));
  bridge->CloseSession(session_id, std::move(close_promise));
  close_loop.Run();
}

}  // namespace media
