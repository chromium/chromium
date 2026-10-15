// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "content/browser/media/cdm_registry_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/common/cdm_info.h"
#include "content/public/test/browser_task_environment.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/cdm_capability.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_type.h"
#include "testing/gmock/include/gmock/gmock.h"
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

const char kTestCdmName[] = "Test CDM";
const char kAlternateCdmName[] = "Alternate CDM";
const media::CdmType kTestCdmType{1234, 5678};
const char kTestPath[] = "/aa/bb";
const char kVersion1[] = "1.1.1.1";
const char kVersion2[] = "1.1.1.2";
const char kTestKeySystem[] = "com.example.somesystem";
const char kOtherKeySystem[] = "com.example.othersystem";
const int kObserver1 = 1;
const int kObserver2 = 2;

#if BUILDFLAG(IS_WIN)
// 0x1234 is randomly picked as a non-software emulated vendor ID.
constexpr uint32_t kNonSoftwareEmulatedVendorId = 0x1234;
// 0x0000 is one of the software emulated vendor IDs.
constexpr uint32_t kSoftwareEmulatedVendorId = 0x0000;
#endif  // BUILDFLAG(IS_WIN)

// Helper function to convert a VideoCodecMap to a list of VideoCodec values
// so that they can be compared. VideoCodecProfiles are ignored.
std::vector<media::VideoCodec> VideoCodecMapToList(
    const media::CdmCapability::VideoCodecMap& map) {
  std::vector<media::VideoCodec> list;
  for (const auto& [video_codec, ignore] : map) {
    list.push_back(video_codec);
  }
  return list;
}

#define EXPECT_STL_EQ(container, ...)                            \
  do {                                                           \
    EXPECT_THAT(container, ::testing::ElementsAre(__VA_ARGS__)); \
  } while (false)

#define EXPECT_AUDIO_CODECS(...) \
  EXPECT_STL_EQ(cdm.capability->audio_codecs, __VA_ARGS__)

#define EXPECT_VIDEO_CODECS(...) \
  EXPECT_STL_EQ(VideoCodecMapToList(cdm.capability->video_codecs), __VA_ARGS__)

#define EXPECT_ENCRYPTION_SCHEMES(...) \
  EXPECT_STL_EQ(cdm.capability->encryption_schemes, __VA_ARGS__)

#define EXPECT_SESSION_TYPES(...) \
  EXPECT_STL_EQ(cdm.capability->session_types, __VA_ARGS__)

}  // namespace

// TODO(crbug.com/347991515): Add browser tests to test protected content id
// settings.
// For simplicity and to make failures easier to diagnose, this test uses
// std::string instead of base::FilePath and std::vector<std::string>.
class CdmRegistryImplTest : public testing::Test {
 public:
  CdmRegistryImplTest() = default;
  ~CdmRegistryImplTest() override = default;

  void SetUp() final {
    DVLOG(1) << __func__;

    if constexpr (BUILDFLAG(IS_CHROMEOS)) {
      // On ChromeOS, only HARDWARE_GL is available, so we cannot disable the
      // ACCELERATED_GL feature without leaving no usable GPU mode.
      GTEST_SKIP();
    }

    auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();

    // Simulate GPU process initialization completing with GL unavailable.
    gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
        gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
    gpu_data_manager->UpdateGpuFeatureInfo(gpu_feature_info, std::nullopt);

#if BUILDFLAG(IS_WIN)
    // Simulate enabling direct composition.
    gpu::GPUInfo gpu_info;
    gpu_info.overlay_info.direct_composition = true;

    // Simulate non-software emulated GPU. Check out `gpu/config/gpu_info.cc` to
    // see which vendor IDs are for the software emulated GPU.
    gpu_info.gpu.vendor_id = kNonSoftwareEmulatedVendorId;
    gpu_data_manager->UpdateGpuInfo(gpu_info, std::nullopt);
#endif  // BUILDFLAG(IS_WIN)

    cdm_registry_.SetCapabilityCBForTesting(capability_cb_.Get());
  }

  void OnKeySystemCapabilitiesUpdated(
      int observer_id,
      base::OnceClosure done_cb,
      KeySystemCapabilities key_system_capabilities) {
    DVLOG(1) << __func__;
    results_[observer_id].push_back(std::move(key_system_capabilities));
    if (PendingLazyInitializationsEmpty() && done_cb) {
      std::move(done_cb).Run();
    }
  }

 protected:
  media::CdmCapability GetTestCdmCapability() {
    return media::CdmCapability(
        {media::AudioCodec::kVorbis},
        {{media::VideoCodec::kVP8, {}}, {media::VideoCodec::kVP9, {}}},
        {EncryptionScheme::kCenc},
        {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense},
        base::Version(kVersion1));
  }

  media::CdmCapability GetOtherCdmCapability() {
    return media::CdmCapability(
        {media::AudioCodec::kVorbis}, {{media::VideoCodec::kVP9, {}}},
        {EncryptionScheme::kCbcs}, {CdmSessionType::kTemporary},
        base::Version(kVersion1));
  }

  CdmInfo GetTestCdmInfo() {
    return CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetTestCdmCapability(),
                   /*supports_sub_key_systems=*/true, kTestCdmName,
                   kTestCdmType, base::FilePath::FromUTF8Unsafe(kTestPath));
  }

  void Register(CdmInfo cdm_info) {
    cdm_registry_.RegisterCdm(std::move(cdm_info));
  }

  void Register(const std::string& key_system,
                std::optional<media::CdmCapability> capability,
                Robustness robustness = Robustness::kSoftwareSecure) {
    Register(CdmInfo(key_system, robustness, std::move(capability),
                     /*supports_sub_key_systems=*/true, kTestCdmName,
                     kTestCdmType, base::FilePath::FromUTF8Unsafe(kTestPath)));
  }

  void RegisterForLazySoftwareSecureInitialization() {
    // Register a CdmInfo without CdmCapability to allow lazy initialization.
    Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                     std::nullopt, kTestCdmType));
    auto cdm_info = cdm_registry_.GetCdmInfo(
        kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
    ASSERT_TRUE(cdm_info);
    ASSERT_FALSE(cdm_info->capability);
  }

  void RegisterForLazyHardwareSecureInitialization() {
    // Register a CdmInfo without CdmCapability to allow lazy initialization.
    Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                     std::nullopt, kTestCdmType));
    auto cdm_info = cdm_registry_.GetCdmInfo(
        kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
    ASSERT_TRUE(cdm_info);
    ASSERT_FALSE(cdm_info->capability);
  }

  bool IsRegistered(const std::string& name, const std::string& version) {
    for (const auto& cdm : cdm_registry_.GetRegisteredCdms()) {
      if (cdm.name == name && cdm.capability->version.GetString() == version) {
        return true;
      }
    }
    return false;
  }

  std::vector<std::string> GetVersions(const media::CdmType& cdm_type) {
    std::vector<std::string> versions;
    for (const auto& cdm : cdm_registry_.GetRegisteredCdms()) {
      if (cdm.type == cdm_type) {
        versions.push_back(cdm.capability->version.GetString());
      }
    }
    return versions;
  }

  bool PendingLazyInitializationsEmpty() const {
    return cdm_registry_.pending_lazy_initializations_.empty();
  }

  void GetKeySystemCapabilities(bool allow_hw_secure_capability_check = true) {
    DVLOG(1) << __func__;
    capabilities_cb_sub_ = {};
    base::RunLoop run_loop;
    capabilities_cb_sub_ = cdm_registry_.ObserveKeySystemCapabilities(
        allow_hw_secure_capability_check,
        base::BindRepeating(
            &CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
            base::Unretained(this), kObserver1, run_loop.QuitClosure()));
    run_loop.Run();
  }

  [[maybe_unused]] gpu::GpuFeatureInfo GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType disabled_feature) {
    gpu::GpuFeatureInfo gpu_feature_info;
    for (auto& status : gpu_feature_info.status_values) {
      status = gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled;
    }
    gpu_feature_info.status_values[disabled_feature] =
        gpu::GpuFeatureStatus::kGpuFeatureStatusDisabled;
    return gpu_feature_info;
  }

  void SelectHardwareSecureDecryption(bool enabled) {
    const std::vector<base::test::FeatureRef> kHardwareSecureFeatures = {
        media::kHardwareSecureDecryption};
    const std::vector<base::test::FeatureRef> kNoFeatures = {};

    auto enabled_features = enabled ? kHardwareSecureFeatures : kNoFeatures;
    auto disabled_features = enabled ? kNoFeatures : kHardwareSecureFeatures;
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

#if BUILDFLAG(IS_ANDROID)
  // On Android checking for key system support can be run on a separate
  // thread. Disable this for testing.
  void DisableMediaDrmQueryInSeparateProcess() {
    scoped_feature_list_.InitAndDisableFeature(
        media::kMediaDrmQueryInSeparateProcess);
  }
#endif

  void ClearCapabilityTestOverride() {
    cdm_registry_.SetCapabilityCBForTesting(base::NullCallback());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;

  CdmRegistryImpl cdm_registry_;
  base::MockCallback<CdmRegistryImpl::CapabilityCB> capability_cb_;
  base::CallbackListSubscription capabilities_cb_sub_;

  // Map of "observer ID" to the list of updated KeySystemCapabilities.
  std::map<int, std::vector<KeySystemCapabilities>> results_;
};

TEST_F(CdmRegistryImplTest, Register) {
  Register(GetTestCdmInfo());

  auto cdms = cdm_registry_.GetRegisteredCdms();
  ASSERT_EQ(1u, cdms.size());
  CdmInfo cdm = cdms[0];
  EXPECT_EQ(kTestCdmName, cdm.name);
  EXPECT_EQ(kVersion1, cdm.capability->version.GetString());
  EXPECT_EQ(kTestPath, cdm.path.MaybeAsASCII());
  EXPECT_EQ(kTestCdmType, cdm.type);
  EXPECT_AUDIO_CODECS(AudioCodec::kVorbis);
  EXPECT_VIDEO_CODECS(VideoCodec::kVP8, VideoCodec::kVP9);
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc);
  EXPECT_SESSION_TYPES(CdmSessionType::kTemporary,
                       CdmSessionType::kPersistentLicense);
  EXPECT_EQ(kTestKeySystem, cdm.key_system);
  EXPECT_TRUE(cdm.supports_sub_key_systems);
  EXPECT_EQ(cdm.robustness, CdmInfo::Robustness::kSoftwareSecure);
}

TEST_F(CdmRegistryImplTest, ReRegister) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));

  // Now register same key system with different values.
  cdm_info.supports_sub_key_systems = false;
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
}

TEST_F(CdmRegistryImplTest, MultipleVersions) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.capability->version = base::Version(kVersion2);
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion2));

  // The first inserted CdmInfo takes effect.
  auto result = cdm_registry_.GetCdmInfo(kTestKeySystem,
                                         CdmInfo::Robustness::kSoftwareSecure);
  ASSERT_EQ(result->capability->version, base::Version(kVersion1));
}

TEST_F(CdmRegistryImplTest, NewVersionInsertedLast) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.capability->version = base::Version(kVersion2);
  Register(cdm_info);

  const std::vector<std::string> versions = GetVersions(kTestCdmType);
  EXPECT_EQ(2u, versions.size());
  EXPECT_EQ(kVersion1, versions[0]);
  EXPECT_EQ(kVersion2, versions[1]);
}

TEST_F(CdmRegistryImplTest, DifferentNames) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.name = kAlternateCdmName;
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
  EXPECT_TRUE(IsRegistered(kAlternateCdmName, kVersion1));
}

TEST_F(CdmRegistryImplTest, Profiles) {
  Register(kTestKeySystem,
           media::CdmCapability(
               {AudioCodec::kVorbis},
               {{VideoCodec::kVP9,
                 media::VideoCodecInfo({media::VP9PROFILE_PROFILE0,
                                        media::VP9PROFILE_PROFILE2})}},
               {EncryptionScheme::kCenc}, {CdmSessionType::kTemporary},
               base::Version(kVersion1)));
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  CdmInfo& cdm = *cdm_info;
  EXPECT_VIDEO_CODECS(VideoCodec::kVP9);
  EXPECT_TRUE(cdm.capability->video_codecs[VideoCodec::kVP9]
                  .supported_profiles.contains(media::VP9PROFILE_PROFILE0));
  EXPECT_TRUE(cdm.capability->video_codecs[VideoCodec::kVP9]
                  .supported_profiles.contains(media::VP9PROFILE_PROFILE2));
  EXPECT_TRUE(
      cdm.capability->video_codecs[VideoCodec::kVP9].supports_clear_lead);
}

TEST_F(CdmRegistryImplTest, SupportedEncryptionSchemes) {
  auto cdm_info = GetTestCdmInfo();
  cdm_info.capability->encryption_schemes = {EncryptionScheme::kCenc,
                                             EncryptionScheme::kCbcs};
  Register(cdm_info);

  std::vector<CdmInfo> cdms = cdm_registry_.GetRegisteredCdms();
  ASSERT_EQ(1u, cdms.size());
  const CdmInfo& cdm = cdms[0];
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc, EncryptionScheme::kCbcs);
}

TEST_F(CdmRegistryImplTest, GetCdmInfo_Success) {
  Register(GetTestCdmInfo());
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  ASSERT_TRUE(cdm_info);

  const CdmInfo& cdm = *cdm_info;

  EXPECT_EQ(kTestCdmName, cdm.name);
  EXPECT_EQ(kVersion1, cdm.capability->version.GetString());
  EXPECT_EQ(kTestPath, cdm.path.MaybeAsASCII());
  EXPECT_EQ(kTestCdmType, cdm.type);
  EXPECT_VIDEO_CODECS(VideoCodec::kVP8, VideoCodec::kVP9);
  EXPECT_ENCRYPTION_SCHEMES(EncryptionScheme::kCenc);
  EXPECT_SESSION_TYPES(CdmSessionType::kTemporary,
                       CdmSessionType::kPersistentLicense);
  EXPECT_EQ(kTestKeySystem, cdm.key_system);
  EXPECT_TRUE(cdm.supports_sub_key_systems);
  EXPECT_EQ(cdm.robustness, CdmInfo::Robustness::kSoftwareSecure);
}

TEST_F(CdmRegistryImplTest, GetCdmInfo_Fail) {
  Register(GetTestCdmInfo());
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  ASSERT_FALSE(cdm_info);
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_SoftwareSecure) {
  Register(GetTestCdmInfo());
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support.hw_cdm_capability_or_status.has_value());
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_HardwareSecure) {
  Register(kTestKeySystem, GetTestCdmCapability(), Robustness::kHardwareSecure);
  SelectHardwareSecureDecryption(true);

  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_FALSE(support.sw_cdm_capability_or_status.has_value());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_LazySoftwareSecureInitialize_Supported) {
  RegisterForLazySoftwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support.hw_cdm_capability_or_status.has_value());
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_LazyHardwareSecureInitialize_Supported) {
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_FALSE(support.sw_cdm_capability_or_status.has_value());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_LazySoftwareSecureInitialize_NotSupported) {
  RegisterForLazySoftwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(media::CdmCapabilityQueryStatus::kUnknown)));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_TRUE(key_system_capabilities.empty());

  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  ASSERT_EQ(cdm_info->status, CdmInfo::Status::kEnabled);
  ASSERT_FALSE(cdm_info->capability);
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_LazyHardwareSecureInitialize_NotSupported) {
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(
          base::unexpected(media::CdmCapabilityQueryStatus::kUnknown)));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_TRUE(key_system_capabilities.empty());

  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  ASSERT_EQ(cdm_info->status, CdmInfo::Status::kEnabled);
  ASSERT_FALSE(cdm_info->capability);
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_HardwareSecureDisabled) {
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(false);
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_TRUE(key_system_capabilities.empty());

  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  ASSERT_EQ(cdm_info->status,
            CdmInfo::Status::kHardwareSecureDecryptionDisabled);
  ASSERT_FALSE(cdm_info->capability);
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_SoftwareAndHardwareSecure) {
  RegisterForLazySoftwareSecureInitialization();
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetOtherCdmCapability()));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetOtherCdmCapability());
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_MultipleObservers) {
  Register(GetTestCdmInfo());

  base::RunLoop run_loop;
  auto subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver1,
                          base::DoNothing()));
  auto subscription2 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver2,
                          run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2][0]);
}

TEST_F(
    CdmRegistryImplTest,
    KeySystemCapabilities_MultipleObservers_PendingLazySoftwareSecureInitialize) {
  RegisterForLazySoftwareSecureInitialization();
  SelectHardwareSecureDecryption(false);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  base::RunLoop run_loop;
  auto subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver1,
                          base::DoNothing()));
  auto subscription2 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver2,
                          run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support.hw_cdm_capability_or_status.has_value());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 2u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2].back());
}

TEST_F(
    CdmRegistryImplTest,
    KeySystemCapabilities_MultipleObservers_PendingLazyHardwareSecureInitialize) {
  Register(GetTestCdmInfo());
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auto subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver1,
                          base::DoNothing()));
  auto subscription2 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        results_[kObserver2].push_back(caps);
        if (PendingLazyInitializationsEmpty() && quit_closure) {
          std::move(quit_closure).Run();
        }
      }));
  run_loop.Run();

  ASSERT_TRUE(results_.count(kObserver1));
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(key_system_capabilities, results_[kObserver2].back());
}

TEST_F(
    CdmRegistryImplTest,
    KeySystemCapabilities_MultipleObservers_AfterLazyHardwareSecureInitialize) {
  Register(GetTestCdmInfo());
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  base::CallbackListSubscription subscription1;
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/true,
        base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
          results_[kObserver1].push_back(caps);
          if (PendingLazyInitializationsEmpty() && quit_closure) {
            std::move(quit_closure).Run();
          }
        }));
    run_loop.Run();
  }
  base::CallbackListSubscription subscription2;
  {
    base::RunLoop run_loop;
    subscription2 = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/true,
        base::BindRepeating(
            &CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
            base::Unretained(this), kObserver2, run_loop.QuitClosure()));
    run_loop.Run();
  }

  ASSERT_TRUE(results_.count(kObserver1));
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2][0]);
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_RegisterCdmAfterObserving) {
  Register(GetTestCdmInfo());
  SelectHardwareSecureDecryption(true);

  base::RunLoop run_loop1;
  auto quit1 = run_loop1.QuitClosure();
  base::RepeatingClosure quit2;

  auto subscription = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        results_[kObserver1].push_back(caps);
        if (quit1) {
          std::move(quit1).Run();
        }
        if (PendingLazyInitializationsEmpty() && quit2) {
          quit2.Run();
        }
      }));
  run_loop1.Run();

  // Hardware security not supported for `kTestKeySystem`.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities_1 = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities_1.size(), 1u);
  ASSERT_TRUE(key_system_capabilities_1.count(kTestKeySystem));
  const auto& support_1 = key_system_capabilities_1[kTestKeySystem];
  ASSERT_EQ(support_1.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support_1.hw_cdm_capability_or_status.has_value());

  {
    base::RunLoop run_loop2;
    quit2 = run_loop2.QuitClosure();
    EXPECT_CALL(capability_cb_,
                Run(kTestKeySystem, Robustness::kHardwareSecure, _))
        .WillOnce(RunOnceCallback<2>(GetOtherCdmCapability()));
    RegisterForLazyHardwareSecureInitialization();
    run_loop2.Run();
  }

  // Now hardware security is supported for `kTestKeySystem`.
  ASSERT_TRUE(results_.count(kObserver1));
  auto& key_system_capabilities_2 = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities_2.size(), 1u);
  ASSERT_TRUE(key_system_capabilities_2.count(kTestKeySystem));
  const auto& support_2 = key_system_capabilities_2[kTestKeySystem];
  ASSERT_EQ(support_2.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_EQ(support_2.hw_cdm_capability_or_status.value(),
            GetOtherCdmCapability());
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_RegisterCdmWithoutActiveObservers) {
  Register(GetTestCdmInfo());

  // 1. First observer subscribes and populates `key_system_capabilities_`
  // cache.
  {
    base::RunLoop run_loop;
    auto subscription = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/false,
        base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
          results_[kObserver1].push_back(caps);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  // The observer is destroyed and `key_system_capabilities_update_callbacks_`
  // is now empty.
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  ASSERT_TRUE(results_[kObserver1][0].count(kTestKeySystem));
  ASSERT_FALSE(results_[kObserver1][0].count(kOtherKeySystem));

  // 2. Register a new CDM while no observers are active.
  Register(kOtherKeySystem, GetOtherCdmCapability());

  // 3. New observer subscribes. It must receive capabilities containing the
  // newly registered CDM.
  {
    base::RunLoop run_loop;
    auto subscription = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/false,
        base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
          results_[kObserver2].push_back(caps);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  EXPECT_TRUE(results_[kObserver2][0].count(kTestKeySystem));
  EXPECT_TRUE(results_[kObserver2][0].count(kOtherKeySystem));
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_RegisterCdmPendingLazyInitialize) {
  SelectHardwareSecureDecryption(true);

  // Save the callbacks so we can control when and how they are fired.
  base::OnceCallback<void(media::CdmCapabilityOrStatus)> callback_test,
      callback_other;
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&callback_test));
  EXPECT_CALL(capability_cb_,
              Run(kOtherKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&callback_other));

  // Register CdmInfo for lazy initialization.
  base::CallbackListSubscription subscription;
  {
    base::RunLoop run_loop;
    Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                     std::nullopt, kTestCdmType));
    subscription = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/true,
        base::BindRepeating(
            &CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
            base::Unretained(this), kObserver1, base::DoNothing()));
    run_loop.RunUntilIdle();
  }

  // An initial update is broadcast immediately with kPending capability.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  EXPECT_TRUE(results_[kObserver1][0][kTestKeySystem].IsPending());

  // Register CdmInfo for another key system while first is still pending.
  // The in-progress lazy initialization for kTestKeySystem is not aborted.
  {
    base::RunLoop run_loop;
    Register(CdmInfo(kOtherKeySystem, CdmInfo::Robustness::kHardwareSecure,
                     std::nullopt, kTestCdmType));
    std::move(callback_test).Run(GetTestCdmCapability());
    std::move(callback_other).Run(GetOtherCdmCapability());
    run_loop.RunUntilIdle();
  }

  // The observer is updated with both key systems once all initialize.
  ASSERT_TRUE(results_.count(kObserver1));
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 2u);

  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_FALSE(support.sw_cdm_capability_or_status.has_value());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());

  ASSERT_TRUE(key_system_capabilities.count(kOtherKeySystem));
  const auto& other_support = key_system_capabilities[kOtherKeySystem];
  ASSERT_FALSE(other_support.sw_cdm_capability_or_status.has_value());
  ASSERT_EQ(other_support.hw_cdm_capability_or_status.value(),
            GetOtherCdmCapability());
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_DisableHardwareSecureCdms) {
  Register(GetTestCdmInfo());
  Register(kTestKeySystem, GetTestCdmCapability(), Robustness::kHardwareSecure);
  SelectHardwareSecureDecryption(true);

  GetKeySystemCapabilities();

  // Both software and hardware security are supported for `kTestKeySystem`.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities_1 = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities_1.size(), 1u);
  ASSERT_TRUE(key_system_capabilities_1.count(kTestKeySystem));
  const auto& support_1 = key_system_capabilities_1[kTestKeySystem];
  ASSERT_EQ(support_1.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_EQ(support_1.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());

  {
    base::RunLoop run_loop;
    cdm_registry_.SetHardwareSecureCdmStatus(CdmInfo::Status::kDisabledOnError);
    run_loop.RunUntilIdle();
  }

  // Now hardware security is NOT supported for `kTestKeySystem`. Software
  // security is not affected.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities_2 = results_[kObserver1][1];
  ASSERT_EQ(key_system_capabilities_2.size(), 1u);
  ASSERT_TRUE(key_system_capabilities_2.count(kTestKeySystem));
  const auto& support_2 = key_system_capabilities_2[kTestKeySystem];
  ASSERT_EQ(support_2.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support_2.hw_cdm_capability_or_status.has_value());
}

TEST_F(
    CdmRegistryImplTest,
    KeySystemCapabilities_DisableHardwareSecureCdms_WhilePendingLazyInitialize) {
  SelectHardwareSecureDecryption(true);

  base::OnceCallback<void(media::CdmCapabilityOrStatus)> callback;
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&callback));

  Register(GetTestCdmInfo());
  RegisterForLazyHardwareSecureInitialization();

  base::CallbackListSubscription subscription;
  {
    base::RunLoop run_loop;
    subscription = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/true,
        base::BindRepeating(
            &CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
            base::Unretained(this), kObserver1, base::DoNothing()));
    run_loop.RunUntilIdle();
  }

  // Initial update: SW is supported, HW check is pending (kPending).
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities_1 = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities_1.size(), 1u);
  const auto& support_1 = key_system_capabilities_1[kTestKeySystem];
  ASSERT_EQ(support_1.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support_1.hw_cdm_capability_or_status.has_value());
  ASSERT_EQ(support_1.hw_cdm_capability_or_status.error(),
            media::CdmCapabilityQueryStatus::kPending);
  ASSERT_TRUE(support_1.IsPending());

  // Hardware secure CDMs are disabled while the check is still pending.
  {
    base::RunLoop run_loop;
    cdm_registry_.SetHardwareSecureCdmStatus(CdmInfo::Status::kDisabledOnError);
    run_loop.RunUntilIdle();
  }

  // Observers should immediately be notified that HW is no longer pending and
  // not supported (kUnknown error, not kPending).
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities_2 = results_[kObserver1][1];
  ASSERT_EQ(key_system_capabilities_2.size(), 1u);
  const auto& support_2 = key_system_capabilities_2[kTestKeySystem];
  ASSERT_EQ(support_2.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_FALSE(support_2.hw_cdm_capability_or_status.has_value());
  ASSERT_EQ(support_2.hw_cdm_capability_or_status.error(),
            media::CdmCapabilityQueryStatus::kUnknown);
  ASSERT_FALSE(support_2.IsPending());

  // When the now-discarded callback finally returns, it should not resurrect or
  // re-enable HW capability.
  {
    base::RunLoop run_loop;
    std::move(callback).Run(GetTestCdmCapability());
    run_loop.RunUntilIdle();
  }

  // No further updates or change in support.
  ASSERT_EQ(results_[kObserver1].size(), 2u);
}

#if BUILDFLAG(IS_WIN)
TEST_F(CdmRegistryImplTest, KeySystemCapabilities_DirectCompositionDisabled) {
  // Simulate disabling direct composition.
  gpu::GPUInfo gpu_info;
  gpu_info.overlay_info.direct_composition = false;
  GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info, std::nullopt);

  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_TRUE(key_system_capabilities.empty());

  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  ASSERT_EQ(cdm_info->status, CdmInfo::Status::kGpuCompositionDisabled);
  ASSERT_FALSE(cdm_info->capability);
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_DisabledBySoftwareEmulatedGpu) {
  // Simulate enabling direct composition and software emulated GPU.
  gpu::GPUInfo gpu_info;
  gpu_info.overlay_info.direct_composition = true;
  gpu_info.gpu.vendor_id = kSoftwareEmulatedVendorId;
  GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info, std::nullopt);

  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_TRUE(key_system_capabilities.empty());

  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  ASSERT_EQ(cdm_info->status, CdmInfo::Status::kDisabledBySoftwareEmulatedGpu);
  ASSERT_FALSE(cdm_info->capability);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_HwCapabilityNotAllowedToAllowed) {
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  // Start with hw capability check not allowed and observe that we don't get
  // capability.
  GetKeySystemCapabilities(/*allow_hw_secure_capability_check=*/false);

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_TRUE(key_system_capabilities.empty());
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);
  EXPECT_EQ(cdm_info->status, CdmInfo::Status::kUninitialized);
  EXPECT_FALSE(cdm_info->capability);

  // Now we allow hw capability check and expect that we get capability.
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));
  GetKeySystemCapabilities(/*allow_hw_secure_capability_check=*/true);

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 3u);
  auto& key_system_capabilities2 = results_[kObserver1].back();
  ASSERT_FALSE(key_system_capabilities2.empty());
  auto cdm_info2 = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);

  EXPECT_EQ(cdm_info2->status, CdmInfo::Status::kEnabled);
  EXPECT_TRUE(cdm_info2->capability);
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_HwCapabilityAllowedToNotAllowed) {
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  // Start with hw capability check allowed and observe that we get capability.
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));
  GetKeySystemCapabilities(/*allow_hw_secure_capability_check=*/true);

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_FALSE(key_system_capabilities.empty());
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);

  EXPECT_EQ(cdm_info->status, CdmInfo::Status::kEnabled);
  EXPECT_EQ(cdm_info->capability, GetTestCdmCapability());

  // Now we don't allow hw capability check, but still expect that we get
  // capability. Note that we don't EXPECT_CALL to capability since the
  // CdmRegistry just returns the cached capability.
  GetKeySystemCapabilities(/*allow_hw_secure_capability_check=*/false);

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 3u);
  auto& key_system_capabilities2 = results_[kObserver1].back();
  ASSERT_FALSE(key_system_capabilities2.empty());
  auto cdm_info2 = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kHardwareSecure);

  EXPECT_EQ(cdm_info2->status, CdmInfo::Status::kEnabled);
  EXPECT_TRUE(cdm_info2->capability);
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_HwCapabilityTwoObserverNotAllowedAndAllowed) {
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  base::RunLoop run_loop;
  auto subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/false,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver1,
                          run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_TRUE(key_system_capabilities.empty());

  base::RunLoop run_loop2;
  auto subscription2 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver2,
                          run_loop2.QuitClosure()));
  run_loop2.Run();

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 2u);
  auto& key_system_capabilities2 = results_[kObserver2].back();
  const auto& support = key_system_capabilities2[kTestKeySystem];
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
}

TEST_F(
    CdmRegistryImplTest,
    KeySystemCapabilities_MultipleObservers_NotAllowedAndAllowedHwCapabilityCheck) {
  RegisterForLazySoftwareSecureInitialization();
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  // The lazy software capability is triggered once; the second observer does
  // not invalidate or retrigger initializations already in progress.
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auto subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/false,
      base::BindRepeating(&CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
                          base::Unretained(this), kObserver1,
                          base::DoNothing()));
  auto subscription2 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        results_[kObserver2].push_back(caps);
        if (PendingLazyInitializationsEmpty() && quit_closure) {
          std::move(quit_closure).Run();
        }
      }));
  run_loop.Run();

  ASSERT_TRUE(results_.count(kObserver1));
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  ASSERT_EQ(support.hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(key_system_capabilities, results_[kObserver2].back());
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_NoOverride) {
#if BUILDFLAG(IS_ANDROID)
  DisableMediaDrmQueryInSeparateProcess();
#endif

  // kTestKeySystem doesn't exist on any platform, but this should at least
  // exercise a bit more of the code (and leave the capabilities as nullptr).
  RegisterForLazySoftwareSecureInitialization();

  // Don't use the testing callback.
  ClearCapabilityTestOverride();
  EXPECT_CALL(capability_cb_, Run(_, _, _)).Times(0);

  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities = results_[kObserver1].back();
  ASSERT_EQ(key_system_capabilities.size(), 0u);
}

// Scenario 1: Immediate software notification — Synchronously finalized
// software CDMs (e.g. Widevine L3) notify observers immediately without
// waiting for pending lazy hardware initializations.
TEST_F(CdmRegistryImplTest,
       ObserveKeySystemCapabilities_ImmediateSoftwareNotification) {
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetTestCdmCapability(), kTestCdmType));
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  base::RunLoop run_loop_sw, run_loop_hw;
  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb;

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb));

  std::vector<KeySystemCapabilities> received_updates;
  auto quit_sw = run_loop_sw.QuitClosure();
  auto quit_hw = run_loop_hw.QuitClosure();

  auto cb_sub = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        received_updates.push_back(caps);
        if (caps.contains(kTestKeySystem)) {
          if (caps[kTestKeySystem].hw_cdm_capability_or_status.has_value()) {
            if (quit_hw) {
              std::move(quit_hw).Run();
            }
          } else if (caps[kTestKeySystem]
                         .sw_cdm_capability_or_status.has_value()) {
            if (quit_sw) {
              std::move(quit_sw).Run();
            }
          }
        }
      }));

  // Software capability must be broadcast immediately without waiting for HW
  // check to complete!
  run_loop_sw.Run();
  ASSERT_EQ(received_updates.size(), 1u);
  auto& sw_capabilities = received_updates[0];
  ASSERT_TRUE(sw_capabilities.contains(kTestKeySystem));
  EXPECT_EQ(sw_capabilities[kTestKeySystem].sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_FALSE(
      sw_capabilities[kTestKeySystem].hw_cdm_capability_or_status.has_value());

  // Now finish the lazy HW initialization in the background.
  std::move(cdm_cb).Run(GetTestCdmCapability());
  run_loop_hw.Run();
  ASSERT_EQ(received_updates.size(), 2u);
  auto& hw_capabilities = received_updates[1];
  EXPECT_EQ(hw_capabilities[kTestKeySystem].sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_EQ(hw_capabilities[kTestKeySystem].hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
}

// Scenario 2: Concurrent different key systems — Two lazy hardware
// initializations notify observers progressively as each finishes without
// waiting for the other.
TEST_F(CdmRegistryImplTest,
       ObserveKeySystemCapabilities_ConcurrentDifferentKeySystems) {
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                   std::nullopt, kTestCdmType));
  Register(CdmInfo(kOtherKeySystem, CdmInfo::Robustness::kHardwareSecure,
                   std::nullopt, kTestCdmType));
  SelectHardwareSecureDecryption(true);

  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_test;
  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_other;

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_test));
  EXPECT_CALL(capability_cb_,
              Run(kOtherKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_other));

  std::vector<KeySystemCapabilities> received_updates;
  base::RunLoop run_loop_test, run_loop_other;
  auto quit_test = run_loop_test.QuitClosure();
  auto quit_other = run_loop_other.QuitClosure();

  auto cb_sub = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        received_updates.push_back(caps);
        if (caps.contains(kTestKeySystem) &&
            caps[kTestKeySystem].hw_cdm_capability_or_status.has_value() &&
            (!caps.contains(kOtherKeySystem) ||
             !caps[kOtherKeySystem].hw_cdm_capability_or_status.has_value())) {
          if (quit_test) {
            std::move(quit_test).Run();
          }
        } else if (caps.contains(kOtherKeySystem) &&
                   caps[kOtherKeySystem]
                       .hw_cdm_capability_or_status.has_value()) {
          if (quit_other) {
            std::move(quit_other).Run();
          }
        }
      }));

  // Resolve kTestKeySystem first: observer receives update with kTestKeySystem!
  std::move(cdm_cb_test).Run(GetTestCdmCapability());
  run_loop_test.Run();
  ASSERT_EQ(received_updates.size(), 2u);
  EXPECT_TRUE(received_updates[1].contains(kTestKeySystem));
  EXPECT_TRUE(received_updates[1][kTestKeySystem]
                  .hw_cdm_capability_or_status.has_value());
  if (received_updates[1].contains(kOtherKeySystem)) {
    EXPECT_FALSE(received_updates[1][kOtherKeySystem]
                     .hw_cdm_capability_or_status.has_value());
  }

  // Now resolve kOtherKeySystem. Observer receives update with both!
  std::move(cdm_cb_other).Run(GetOtherCdmCapability());
  run_loop_other.Run();
  ASSERT_EQ(received_updates.size(), 3u);
  EXPECT_TRUE(received_updates[2][kTestKeySystem]
                  .hw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(received_updates[2][kOtherKeySystem]
                  .hw_cdm_capability_or_status.has_value());
}

// Scenario 3: Static software only with lazy hardware check.
// Verifies that a key system with static software capabilities and an on-going
// lazy hardware check reports IsPending() as true while an unregistered
// hardware CDM reports IsPending() as false.
TEST_F(CdmRegistryImplTest,
       ObserveKeySystemCapabilities_StaticSoftwareWithLazyHardware) {
  // Register kTestKeySystem with static SW and lazy HW.
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetTestCdmCapability(), kTestCdmType));
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                   std::nullopt, kTestCdmType));
  // Register kOtherKeySystem with only static SW (e.g. Widevine on desktop).
  Register(CdmInfo(kOtherKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetOtherCdmCapability(), kTestCdmType));
  SelectHardwareSecureDecryption(true);

  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_hw;
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_hw));

  std::vector<KeySystemCapabilities> received_updates;
  base::RunLoop run_loop_sw, run_loop_hw;
  auto quit_sw = run_loop_sw.QuitClosure();
  auto quit_hw = run_loop_hw.QuitClosure();

  auto cb_sub = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        received_updates.push_back(caps);
        if (caps.contains(kTestKeySystem)) {
          if (caps[kTestKeySystem].hw_cdm_capability_or_status.has_value()) {
            if (quit_hw) {
              std::move(quit_hw).Run();
            }
          } else if (caps[kTestKeySystem]
                         .sw_cdm_capability_or_status.has_value()) {
            if (quit_sw) {
              std::move(quit_sw).Run();
            }
          }
        }
      }));

  // 1st Broadcast: Software capabilities must be immediately broadcast.
  run_loop_sw.Run();
  ASSERT_EQ(received_updates.size(), 1u);
  auto& initial_caps = received_updates[0];

  // kOtherKeySystem has no hardware CDM registered and is not pending.
  ASSERT_TRUE(initial_caps.contains(kOtherKeySystem));
  EXPECT_TRUE(
      initial_caps[kOtherKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(
      initial_caps[kOtherKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(initial_caps[kOtherKeySystem].IsPending());

  // kTestKeySystem has hardware check on-going -> IsPending() must be true and
  // hw_cdm_capability_or_status has error kPending.
  ASSERT_TRUE(initial_caps.contains(kTestKeySystem));
  EXPECT_TRUE(
      initial_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(
      initial_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(initial_caps[kTestKeySystem].IsPending());
  EXPECT_EQ(initial_caps[kTestKeySystem].hw_cdm_capability_or_status.error(),
            media::CdmCapabilityQueryStatus::kPending);

  // 2nd Progressive Broadcast: Hardware check finishes.
  std::move(cdm_cb_hw).Run(GetTestCdmCapability());
  run_loop_hw.Run();
  ASSERT_EQ(received_updates.size(), 2u);
  auto& final_caps = received_updates[1];

  EXPECT_TRUE(
      final_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(
      final_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_EQ(final_caps[kTestKeySystem].hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_FALSE(final_caps[kTestKeySystem].IsPending());
}

// Scenario 4a: Dual lazy software and hardware check (with static CDM).
// Verifies that a key system with both software and hardware checks running
// lazily reports IsPending() as true initially, and preserves IsPending() as
// true across progressive updates until both checks complete.
TEST_F(CdmRegistryImplTest,
       ObserveKeySystemCapabilities_DualLazySoftwareAndHardware) {
  // Register kTestKeySystem with both SW and HW lazy uninitialized.
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   std::nullopt, kTestCdmType));
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                   std::nullopt, kTestCdmType));
  // Register kOtherKeySystem with static SW (representing Widevine).
  Register(CdmInfo(kOtherKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetOtherCdmCapability(), kTestCdmType));
  SelectHardwareSecureDecryption(true);

  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_sw;
  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_hw;

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_sw));
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_hw));

  std::vector<KeySystemCapabilities> received_updates;
  base::RunLoop run_loop_sw, run_loop_hw;
  auto quit_sw = run_loop_sw.QuitClosure();
  auto quit_hw = run_loop_hw.QuitClosure();

  auto cb_sub = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        received_updates.push_back(caps);
        if (caps.contains(kTestKeySystem)) {
          if (caps[kTestKeySystem].hw_cdm_capability_or_status.has_value()) {
            if (quit_hw) {
              std::move(quit_hw).Run();
            }
          } else if (caps[kTestKeySystem]
                         .sw_cdm_capability_or_status.has_value()) {
            if (quit_sw) {
              std::move(quit_sw).Run();
            }
          }
        }
      }));

  // 1st Broadcast: Both SW and HW checks are in progress.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(received_updates.size(), 1u);
  auto& initial_caps = received_updates[0];
  ASSERT_TRUE(initial_caps.contains(kTestKeySystem));
  EXPECT_FALSE(
      initial_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(
      initial_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(initial_caps[kTestKeySystem].IsPending());
  EXPECT_EQ(initial_caps[kTestKeySystem].sw_cdm_capability_or_status.error(),
            media::CdmCapabilityQueryStatus::kPending);
  EXPECT_EQ(initial_caps[kTestKeySystem].hw_cdm_capability_or_status.error(),
            media::CdmCapabilityQueryStatus::kPending);

  // Progressive Broadcast 1: SW check completes first.
  std::move(cdm_cb_sw).Run(GetTestCdmCapability());
  run_loop_sw.Run();
  ASSERT_EQ(received_updates.size(), 2u);
  auto& interim_caps = received_updates[1];
  EXPECT_TRUE(
      interim_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  // HW is still in progress.
  EXPECT_FALSE(
      interim_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(interim_caps[kTestKeySystem].IsPending());
  EXPECT_EQ(interim_caps[kTestKeySystem].hw_cdm_capability_or_status.error(),
            media::CdmCapabilityQueryStatus::kPending);

  // Progressive Broadcast 2: HW check completes.
  std::move(cdm_cb_hw).Run(GetTestCdmCapability());
  run_loop_hw.Run();
  ASSERT_EQ(received_updates.size(), 3u);
  auto& final_caps = received_updates.back();
  EXPECT_TRUE(
      final_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(
      final_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_EQ(final_caps[kTestKeySystem].sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_EQ(final_caps[kTestKeySystem].hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_FALSE(final_caps[kTestKeySystem].IsPending());
}

// Scenario 4b: Dual lazy software and hardware check with no other CDMs.
// Verifies that when a key system with dual lazy checks is the only registered
// CDM (i.e. no Widevine or other static CDM present), CdmRegistryImpl still
// immediately broadcasts the initial snapshot with IsPending() = true. This
// enables renderers to receive the initial update and immediately reject
// queries for unrelated/absent key systems without waiting for the slow check.
TEST_F(CdmRegistryImplTest,
       ObserveKeySystemCapabilities_DualLazySoftwareAndHardwareOnly) {
  // Register kTestKeySystem with both SW and HW lazy uninitialized.
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   std::nullopt, kTestCdmType));
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                   std::nullopt, kTestCdmType));
  SelectHardwareSecureDecryption(true);

  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_sw;
  base::OnceCallback<void(media::CdmCapabilityOrStatus)> cdm_cb_hw;

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_sw));
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&cdm_cb_hw));

  std::vector<KeySystemCapabilities> received_updates;
  base::RunLoop run_loop_sw, run_loop_hw;
  auto quit_sw = run_loop_sw.QuitClosure();
  auto quit_hw = run_loop_hw.QuitClosure();

  auto cb_sub = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        received_updates.push_back(caps);
        if (caps.contains(kTestKeySystem)) {
          if (caps[kTestKeySystem].hw_cdm_capability_or_status.has_value()) {
            if (quit_hw) {
              std::move(quit_hw).Run();
            }
          } else if (caps[kTestKeySystem]
                         .sw_cdm_capability_or_status.has_value()) {
            if (quit_sw) {
              std::move(quit_sw).Run();
            }
          }
        }
      }));

  // 1st Broadcast: Both SW and HW checks are in progress, but the initial
  // snapshot must be broadcast immediately even with no static CDMs.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(received_updates.size(), 1u);
  auto& initial_caps = received_updates[0];
  ASSERT_TRUE(initial_caps.contains(kTestKeySystem));
  EXPECT_FALSE(
      initial_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(
      initial_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(initial_caps[kTestKeySystem].IsPending());

  // Progressive Broadcast 1: SW check completes first.
  std::move(cdm_cb_sw).Run(GetTestCdmCapability());
  run_loop_sw.Run();
  ASSERT_EQ(received_updates.size(), 2u);
  auto& interim_caps = received_updates[1];
  EXPECT_TRUE(
      interim_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(
      interim_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(interim_caps[kTestKeySystem].IsPending());

  // Progressive Broadcast 2: HW check completes.
  std::move(cdm_cb_hw).Run(GetTestCdmCapability());
  run_loop_hw.Run();
  ASSERT_EQ(received_updates.size(), 3u);
  auto& final_caps = received_updates.back();
  EXPECT_TRUE(
      final_caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_TRUE(
      final_caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_EQ(final_caps[kTestKeySystem].sw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_EQ(final_caps[kTestKeySystem].hw_cdm_capability_or_status.value(),
            GetTestCdmCapability());
  EXPECT_FALSE(final_caps[kTestKeySystem].IsPending());
}

// Scenario 5: Static software with disabled hardware secure decryption.
// Verifies that when hardware secure decryption is disabled (e.g. disabled
// accelerated video decoding), IsPending() is false and the key system is
// immediately ready.
TEST_F(CdmRegistryImplTest,
       ObserveKeySystemCapabilities_StaticSoftwareWithDisabledHardware) {
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetTestCdmCapability(), kTestCdmType));
  Register(CdmInfo(kTestKeySystem, CdmInfo::Robustness::kHardwareSecure,
                   std::nullopt, kTestCdmType));
  SelectHardwareSecureDecryption(false);

  std::vector<KeySystemCapabilities> received_updates;
  base::RunLoop run_loop;
  auto cb_sub = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/true,
      base::BindLambdaForTesting([&](KeySystemCapabilities caps) {
        received_updates.push_back(caps);
        run_loop.Quit();
      }));

  run_loop.Run();
  ASSERT_EQ(received_updates.size(), 1u);
  auto& caps = received_updates[0];
  ASSERT_TRUE(caps.contains(kTestKeySystem));
  EXPECT_TRUE(caps[kTestKeySystem].sw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(caps[kTestKeySystem].hw_cdm_capability_or_status.has_value());
  EXPECT_FALSE(caps[kTestKeySystem].IsPending());
}

}  // namespace content
