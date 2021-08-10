// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/token.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using AudioCodec = media::AudioCodec;
using VideoCodec = media::VideoCodec;
using EncryptionScheme = media::EncryptionScheme;
using CdmSessionType = media::CdmSessionType;
using Robustness = CdmInfo::Robustness;
using base::test::RunOnceCallback;
using testing::_;

const char kTestCdmName[] = "Test Content Decryption Module";
const base::Token kTestCdmGuid{1234, 5678};
const char kVersion[] = "1.1.1.1";
const char kTestPath[] = "/aa/bb";
const char kTestFileSystemId[] = "file_system_id";

// Helper function to convert a VideoCodecMap to a list of VideoCodec values
// so that they can be compared. VideoCodecProfiles are ignored.
std::vector<media::VideoCodec> VideoCodecMapToList(
    const media::CdmCapability::VideoCodecMap& map) {
  std::vector<media::VideoCodec> list;
  for (const auto& entry : map) {
    list.push_back(entry.first);
  }
  return list;
}

#define EXPECT_STL_EQ(container, ...)                            \
  do {                                                           \
    EXPECT_THAT(container, ::testing::ElementsAre(__VA_ARGS__)); \
  } while (false)

#define EXPECT_AUDIO_CODECS(...) \
  EXPECT_STL_EQ(capability_->sw_secure_capability->audio_codecs, __VA_ARGS__)

#define EXPECT_VIDEO_CODECS(...)                                            \
  EXPECT_STL_EQ(                                                            \
      VideoCodecMapToList(capability_->sw_secure_capability->video_codecs), \
      __VA_ARGS__)

#define EXPECT_ENCRYPTION_SCHEMES(...)                                 \
  EXPECT_STL_EQ(capability_->sw_secure_capability->encryption_schemes, \
                __VA_ARGS__)

#define EXPECT_SESSION_TYPES(...) \
  EXPECT_STL_EQ(capability_->sw_secure_capability->session_types, __VA_ARGS__)

#define EXPECT_HW_SECURE_AUDIO_CODECS(...) \
  EXPECT_STL_EQ(capability_->hw_secure_capability->audio_codecs, __VA_ARGS__)

#define EXPECT_HW_SECURE_VIDEO_CODECS(...)                                  \
  EXPECT_STL_EQ(                                                            \
      VideoCodecMapToList(capability_->hw_secure_capability->video_codecs), \
      __VA_ARGS__)

#define EXPECT_HW_SECURE_ENCRYPTION_SCHEMES(...)                       \
  EXPECT_STL_EQ(capability_->hw_secure_capability->encryption_schemes, \
                __VA_ARGS__)

#define EXPECT_HW_SECURE_SESSION_TYPES(...) \
  EXPECT_STL_EQ(capability_->hw_secure_capability->session_types, __VA_ARGS__)

}  // namespace

class KeySystemSupportImplTest : public testing::Test {
 protected:
  void SetUp() final {
    DVLOG(1) << __func__;
    // As `CdmRegistryImpl::GetInstance()` is a static, explicitly reset
    // `CdmRegistryImpl` so each test starts with a clean state.
    CdmRegistryImpl::GetInstance()->ResetForTesting();

    auto key_system_support_impl = std::make_unique<KeySystemSupportImpl>();
    key_system_support_impl->SetHardwareSecureCapabilityCBForTesting(
        hw_secure_capability_cb_.Get());

    mojo::MakeSelfOwnedReceiver(
        std::move(key_system_support_impl),
        key_system_support_.BindNewPipeAndPassReceiver());
  }

  media::CdmCapability TestCdmCapability() {
    return media::CdmCapability(
        {AudioCodec::kCodecVorbis},
        {{VideoCodec::kCodecVP8, {}}, {VideoCodec::kCodecVP9, {}}},
        {EncryptionScheme::kCenc, EncryptionScheme::kCbcs},
        {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense});
  }

  // Registers |key_system| with |capability|. All other values for CdmInfo have
  // some default value as they're not returned by IsKeySystemSupported().
  void Register(const std::string& key_system,
                absl::optional<media::CdmCapability> capability,
                Robustness robustness = Robustness::kSoftwareSecure) {
    DVLOG(1) << __func__;

    CdmRegistry::GetInstance()->RegisterCdm(
        CdmInfo(key_system, robustness, std::move(capability),
                /*supports_sub_key_systems=*/false, kTestCdmName, kTestCdmGuid,
                base::Version(kVersion),
                base::FilePath::FromUTF8Unsafe(kTestPath), kTestFileSystemId));
  }

  // Determines if |key_system| is registered. If it is, updates |codecs_|
  // and |persistent_|.
  bool IsSupported(const std::string& key_system) {
    DVLOG(1) << __func__;
    bool is_supported = false;
    key_system_support_->IsKeySystemSupported(key_system, &is_supported,
                                              &capability_);
    return is_supported;
  }

  gpu::GpuFeatureInfo ALLOW_UNUSED_TYPE
  GetGpuFeatureInfoWithOneDisabled(gpu::GpuFeatureType disabled_feature) {
    gpu::GpuFeatureInfo gpu_feature_info;
    for (auto& status : gpu_feature_info.status_values)
      status = gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled;
    gpu_feature_info.status_values[disabled_feature] =
        gpu::GpuFeatureStatus::kGpuFeatureStatusDisabled;
    return gpu_feature_info;
  }

  void SelectHardwareSecureDecryption(bool enabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kLacrosUseChromeosProtectedMedia);
    } else {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kLacrosUseChromeosProtectedMedia);
    }
#else
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          media::kHardwareSecureDecryption);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          media::kHardwareSecureDecryption);
    }
#endif
  }

  mojo::Remote<media::mojom::KeySystemSupport> key_system_support_;
  base::MockCallback<KeySystemSupportImpl::HardwareSecureCapabilityCB>
      hw_secure_capability_cb_;
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;

  // Updated by IsSupported().
  media::mojom::KeySystemCapabilityPtr capability_;
};

TEST_F(KeySystemSupportImplTest, NoKeySystems) {
  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);
}

TEST_F(KeySystemSupportImplTest, SoftwareSecureCapability) {
  Register("KeySystem", TestCdmCapability());

  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_TRUE(capability_->sw_secure_capability);
  EXPECT_FALSE(capability_->hw_secure_capability);
  EXPECT_AUDIO_CODECS(AudioCodec::kCodecVorbis);
  EXPECT_VIDEO_CODECS(VideoCodec::kCodecVP8, VideoCodec::kCodecVP9);
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc, EncryptionScheme::kCbcs);
  EXPECT_SESSION_TYPES(CdmSessionType::kTemporary,
                       CdmSessionType::kPersistentLicense);
}

TEST_F(KeySystemSupportImplTest,
       HardwareSecureCapability_HardwareSecureDecryptionDisabled) {
  SelectHardwareSecureDecryption(false);
  Register("KeySystem", TestCdmCapability(), Robustness::kHardwareSecure);

  EXPECT_FALSE(IsSupported("KeySystem"));
}

TEST_F(KeySystemSupportImplTest, HardwareSecureCapability) {
  SelectHardwareSecureDecryption(true);
  Register("KeySystem", TestCdmCapability(), Robustness::kHardwareSecure);

  // Simulate GPU process initialization completing with GL unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
  GpuDataManagerImpl::GetInstance()->UpdateGpuFeatureInfo(gpu_feature_info,
                                                          absl::nullopt);

  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_->sw_secure_capability);
  EXPECT_TRUE(capability_->hw_secure_capability);
  EXPECT_HW_SECURE_AUDIO_CODECS(AudioCodec::kCodecVorbis);
  EXPECT_HW_SECURE_VIDEO_CODECS(VideoCodec::kCodecVP8, VideoCodec::kCodecVP9);
  EXPECT_HW_SECURE_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc,
                                      EncryptionScheme::kCbcs);
  EXPECT_HW_SECURE_SESSION_TYPES(CdmSessionType::kTemporary,
                                 CdmSessionType::kPersistentLicense);
}

TEST_F(KeySystemSupportImplTest, Profiles) {
  Register("KeySystem",
           media::CdmCapability(
               {AudioCodec::kCodecVorbis},
               {{VideoCodec::kCodecVP9,
                 {media::VP9PROFILE_PROFILE0, media::VP9PROFILE_PROFILE2}}},
               {EncryptionScheme::kCenc}, {CdmSessionType::kTemporary}));

  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_TRUE(capability_->sw_secure_capability);
  EXPECT_VIDEO_CODECS(VideoCodec::kCodecVP9);
  EXPECT_TRUE(base::Contains(
      capability_->sw_secure_capability->video_codecs[VideoCodec::kCodecVP9],
      media::VP9PROFILE_PROFILE0));
  EXPECT_TRUE(base::Contains(
      capability_->sw_secure_capability->video_codecs[VideoCodec::kCodecVP9],
      media::VP9PROFILE_PROFILE2));
}

TEST_F(KeySystemSupportImplTest, MultipleKeySystems) {
  Register("KeySystem1", TestCdmCapability());
  Register("KeySystem2", TestCdmCapability());

  EXPECT_TRUE(IsSupported("KeySystem1"));
  EXPECT_TRUE(IsSupported("KeySystem2"));
}

TEST_F(KeySystemSupportImplTest, MissingKeySystem) {
  Register("KeySystem", TestCdmCapability());

  EXPECT_FALSE(IsSupported("KeySystem1"));
  EXPECT_FALSE(capability_);
}

TEST_F(KeySystemSupportImplTest, LazyInitialize_Supported) {
  SelectHardwareSecureDecryption(true);
  Register("KeySystem", absl::nullopt, Robustness::kHardwareSecure);

  // Simulate GPU process initialization completing with GL unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
  GpuDataManagerImpl::GetInstance()->UpdateGpuFeatureInfo(gpu_feature_info,
                                                          absl::nullopt);

  EXPECT_CALL(hw_secure_capability_cb_, Run("KeySystem", _))
      .WillOnce(RunOnceCallback<1>(TestCdmCapability()));
  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_TRUE(capability_);

  // Calling IsSupported() again should not trigger `hw_secure_capability_cb_`.
  EXPECT_TRUE(IsSupported("KeySystem"));
  EXPECT_TRUE(capability_);
}

TEST_F(KeySystemSupportImplTest, LazyInitialize_NotSupported) {
  SelectHardwareSecureDecryption(true);
  Register("KeySystem", absl::nullopt, Robustness::kHardwareSecure);

  // Simulate GPU process initialization completing with GL unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
  GpuDataManagerImpl::GetInstance()->UpdateGpuFeatureInfo(gpu_feature_info,
                                                          absl::nullopt);

  EXPECT_CALL(hw_secure_capability_cb_, Run("KeySystem", _))
      .WillOnce(RunOnceCallback<1>(absl::nullopt));
  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);

  // Calling IsSupported() again should not trigger `hw_secure_capability_cb_`.
  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);
}

TEST_F(KeySystemSupportImplTest,
       LazyInitialize_HardwareSecureDecryptionDisabled) {
  SelectHardwareSecureDecryption(false);
  Register("KeySystem", absl::nullopt, Robustness::kHardwareSecure);

  EXPECT_FALSE(IsSupported("KeySystem"));
  EXPECT_FALSE(capability_);
}

}  // namespace content
