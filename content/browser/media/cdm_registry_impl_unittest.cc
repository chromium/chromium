// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/media/cdm_registry_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
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
    std::move(done_cb).Run();
  }

 protected:
  media::CdmCapability GetTestCdmCapability() {
    return media::CdmCapability(
        {media::AudioCodec::kVorbis},
        {{media::VideoCodec::kVP8, {}}, {media::VideoCodec::kVP9, {}}},
        {EncryptionScheme::kCenc},
        {CdmSessionType::kTemporary, CdmSessionType::kPersistentLicense});
  }

  media::CdmCapability GetOtherCdmCapability() {
    return media::CdmCapability(
        {media::AudioCodec::kVorbis}, {{media::VideoCodec::kVP9, {}}},
        {EncryptionScheme::kCbcs}, {CdmSessionType::kTemporary});
  }

  CdmInfo GetTestCdmInfo() {
    return CdmInfo(kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure,
                   GetTestCdmCapability(),
                   /*supports_sub_key_systems=*/true, kTestCdmName,
                   kTestCdmType, base::Version(kVersion1),
                   base::FilePath::FromUTF8Unsafe(kTestPath));
  }

  void Register(CdmInfo cdm_info) {
    cdm_registry_.RegisterCdm(std::move(cdm_info));
  }

  void Register(const std::string& key_system,
                std::optional<media::CdmCapability> capability,
                Robustness robustness = Robustness::kSoftwareSecure) {
    Register(CdmInfo(key_system, robustness, std::move(capability),
                     /*supports_sub_key_systems=*/true, kTestCdmName,
                     kTestCdmType, base::Version(kVersion1),
                     base::FilePath::FromUTF8Unsafe(kTestPath)));
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
      if (cdm.name == name && cdm.version.GetString() == version) {
        return true;
      }
    }
    return false;
  }

  std::vector<std::string> GetVersions(const media::CdmType& cdm_type) {
    std::vector<std::string> versions;
    for (const auto& cdm : cdm_registry_.GetRegisteredCdms()) {
      if (cdm.type == cdm_type) {
        versions.push_back(cdm.version.GetString());
      }
    }
    return versions;
  }

  void GetKeySystemCapabilities(bool allow_hw_secure_capability_check = true) {
    DVLOG(1) << __func__;
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kLacrosUseChromeosProtectedMedia);
    } else {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kLacrosUseChromeosProtectedMedia);
    }
#else
    const std::vector<base::test::FeatureRef> kHardwareSecureFeatures = {
        media::kHardwareSecureDecryption,
        media::kHardwareSecureDecryptionExperiment};
    const std::vector<base::test::FeatureRef> kNoFeatures = {};

    auto enabled_features = enabled ? kHardwareSecureFeatures : kNoFeatures;
    auto disabled_features = enabled ? kNoFeatures : kHardwareSecureFeatures;
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
#endif
  }

#if BUILDFLAG(IS_ANDROID)
  // On Android checking for key system support can be run on a separate
  // thread. Disable this for testing.
  void DisableMediaCodecCallsInSeparateThread() {
    scoped_feature_list_.InitAndDisableFeature(
        media::kAllowMediaCodecCallsInSeparateProcess);
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
  EXPECT_EQ(kVersion1, cdm.version.GetString());
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
  cdm_info.version = base::Version(kVersion2);
  Register(cdm_info);

  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion1));
  EXPECT_TRUE(IsRegistered(kTestCdmName, kVersion2));

  // The first inserted CdmInfo takes effect.
  auto result = cdm_registry_.GetCdmInfo(kTestKeySystem,
                                         CdmInfo::Robustness::kSoftwareSecure);
  ASSERT_EQ(result->version, base::Version(kVersion1));
}

TEST_F(CdmRegistryImplTest, NewVersionInsertedLast) {
  auto cdm_info = GetTestCdmInfo();
  Register(cdm_info);
  cdm_info.version = base::Version(kVersion2);
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
               {EncryptionScheme::kCenc}, {CdmSessionType::kTemporary}));
  auto cdm_info = cdm_registry_.GetCdmInfo(
      kTestKeySystem, CdmInfo::Robustness::kSoftwareSecure);
  CdmInfo& cdm = *cdm_info;
  EXPECT_VIDEO_CODECS(VideoCodec::kVP9);
  EXPECT_TRUE(base::Contains(
      cdm.capability->video_codecs[VideoCodec::kVP9].supported_profiles,
      media::VP9PROFILE_PROFILE0));
  EXPECT_TRUE(base::Contains(
      cdm.capability->video_codecs[VideoCodec::kVP9].supported_profiles,
      media::VP9PROFILE_PROFILE2));
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
  EXPECT_EQ(kVersion1, cdm.version.GetString());
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
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_FALSE(support.hw_secure_capability);
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
  ASSERT_FALSE(support.sw_secure_capability);
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());
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
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_FALSE(support.hw_secure_capability);
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
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_FALSE(support.sw_secure_capability);
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_LazySoftwareSecureInitialize_NotSupported) {
  RegisterForLazySoftwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .WillOnce(RunOnceCallback<2>(std::nullopt));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
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
      .WillOnce(RunOnceCallback<2>(std::nullopt));
  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
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
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_EQ(support.hw_secure_capability.value(), GetOtherCdmCapability());
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
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());

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
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_FALSE(support.hw_secure_capability);

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2][0]);
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
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2][0]);
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
    subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/true,
        base::BindRepeating(
            &CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
            base::Unretained(this), kObserver1, run_loop.QuitClosure()));
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
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 1u);
  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2][0]);
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_RegisterCdmAfterObserving) {
  Register(GetTestCdmInfo());
  SelectHardwareSecureDecryption(true);

  base::CallbackListSubscription subscription;
  {
    base::RunLoop run_loop;
    subscription = cdm_registry_.ObserveKeySystemCapabilities(
        /*allow_hw_secure_capability_check=*/true,
        base::BindRepeating(
            &CdmRegistryImplTest::OnKeySystemCapabilitiesUpdated,
            base::Unretained(this), kObserver1, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Hardware security not supported for `kTestKeySystem`.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities_1 = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities_1.size(), 1u);
  ASSERT_TRUE(key_system_capabilities_1.count(kTestKeySystem));
  const auto& support_1 = key_system_capabilities_1[kTestKeySystem];
  ASSERT_EQ(support_1.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_FALSE(support_1.hw_secure_capability);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(capability_cb_,
                Run(kTestKeySystem, Robustness::kHardwareSecure, _))
        .WillOnce(RunOnceCallback<2>(GetOtherCdmCapability()));
    RegisterForLazyHardwareSecureInitialization();
    run_loop.RunUntilIdle();
  }

  // Now hardware security is supported for `kTestKeySystem`.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities_2 = results_[kObserver1][1];
  ASSERT_EQ(key_system_capabilities_2.size(), 1u);
  ASSERT_TRUE(key_system_capabilities_2.count(kTestKeySystem));
  const auto& support_2 = key_system_capabilities_2[kTestKeySystem];
  ASSERT_EQ(support_2.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_EQ(support_2.hw_secure_capability.value(), GetOtherCdmCapability());
}

TEST_F(CdmRegistryImplTest,
       KeySystemCapabilities_RegisterCdmPendingLazyInitialize) {
  SelectHardwareSecureDecryption(true);

  // Save the callbacks so we can control when and how they are fired.
  base::OnceCallback<void(std::optional<media::CdmCapability>)> callback_1,
      callback_2, callback_3;
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&callback_1))
      .WillOnce(MoveArg<2>(&callback_2));
  EXPECT_CALL(capability_cb_,
              Run(kOtherKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(MoveArg<2>(&callback_3));

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

  // No results since we are still pending lazy initialization.
  ASSERT_TRUE(results_.empty());

  // Register CdmInfo for the same key system with a CdmCapability.
  {
    base::RunLoop run_loop;
    // Register a CdmInfo without CdmCapability to allow lazy initialization.
    Register(CdmInfo(kOtherKeySystem, CdmInfo::Robustness::kHardwareSecure,
                     std::nullopt, kTestCdmType));
    std::move(callback_1).Run(GetTestCdmCapability());
    std::move(callback_2).Run(GetTestCdmCapability());
    std::move(callback_3).Run(GetOtherCdmCapability());
    run_loop.RunUntilIdle();
  }

  // The observer should only be updated once with both key systems.
  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 2u);

  ASSERT_TRUE(key_system_capabilities.count(kTestKeySystem));
  const auto& support = key_system_capabilities[kTestKeySystem];
  ASSERT_FALSE(support.sw_secure_capability.has_value());
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());

  ASSERT_TRUE(key_system_capabilities.count(kOtherKeySystem));
  const auto& other_support = key_system_capabilities[kOtherKeySystem];
  ASSERT_FALSE(other_support.sw_secure_capability.has_value());
  ASSERT_EQ(other_support.hw_secure_capability.value(),
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
  ASSERT_EQ(support_1.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_EQ(support_1.hw_secure_capability.value(), GetTestCdmCapability());

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
  ASSERT_EQ(support_2.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_FALSE(support_2.hw_secure_capability);
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
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities2 = results_[kObserver1][1];
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
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
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
  ASSERT_EQ(results_[kObserver1].size(), 2u);
  auto& key_system_capabilities2 = results_[kObserver1][1];
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
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  auto& key_system_capabilities2 = results_[kObserver2][0];
  const auto& support = key_system_capabilities2[kTestKeySystem];
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());
}

TEST_F(
    CdmRegistryImplTest,
    KeySystemCapabilities_MultipleObservers_NotAllowedAndAllowedHwCapabilityCheck) {
  RegisterForLazySoftwareSecureInitialization();
  RegisterForLazyHardwareSecureInitialization();
  SelectHardwareSecureDecryption(true);

  // Expect the lazy software capability to be triggered twice because the
  // second observer will invalidate pending initializations and retrigger them.
  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kSoftwareSecure, _))
      .Times(2)
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<2>(GetTestCdmCapability()));

  EXPECT_CALL(capability_cb_,
              Run(kTestKeySystem, Robustness::kHardwareSecure, _))
      .WillOnce(RunOnceCallback<2>(GetTestCdmCapability()));

  base::RunLoop run_loop;
  auto subscription1 = cdm_registry_.ObserveKeySystemCapabilities(
      /*allow_hw_secure_capability_check=*/false,
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
  ASSERT_EQ(support.sw_secure_capability.value(), GetTestCdmCapability());
  ASSERT_EQ(support.hw_secure_capability.value(), GetTestCdmCapability());

  ASSERT_TRUE(results_.count(kObserver2));
  ASSERT_EQ(results_[kObserver2].size(), 1u);
  ASSERT_EQ(key_system_capabilities, results_[kObserver2][0]);
}

TEST_F(CdmRegistryImplTest, KeySystemCapabilities_NoOverride) {
#if BUILDFLAG(IS_ANDROID)
  DisableMediaCodecCallsInSeparateThread();
#endif

  // kTestKeySystem doesn't exist on any platform, but this should at least
  // exercise a bit more of the code (and leave the capabilities as nullptr).
  RegisterForLazySoftwareSecureInitialization();

  // Don't use the testing callback.
  ClearCapabilityTestOverride();
  EXPECT_CALL(capability_cb_, Run(_, _, _)).Times(0);

  GetKeySystemCapabilities();

  ASSERT_TRUE(results_.count(kObserver1));
  ASSERT_EQ(results_[kObserver1].size(), 1u);
  auto& key_system_capabilities = results_[kObserver1][0];
  ASSERT_EQ(key_system_capabilities.size(), 0u);
}

}  // namespace content
