// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include <drm_fourcc.h>
#include <gbm.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <va/va_str.h>
#include <xf86drm.h>

#include <map>
#include <optional>
#include <vector>

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/base/platform_features.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/linux/gbm_defines.h"

#ifndef I915_FORMAT_MOD_4_TILED
#define I915_FORMAT_MOD_4_TILED 0x100000000000009
#endif

namespace media {
namespace {

std::optional<VAProfile> ConvertToVAProfile(VideoCodecProfile profile) {
  // A map between VideoCodecProfile and VAProfile.
  const std::map<VideoCodecProfile, VAProfile> kProfileMap = {
    // VAProfileH264Baseline is deprecated in <va/va.h> from libva 2.0.0.
    {H264PROFILE_BASELINE, VAProfileH264ConstrainedBaseline},
    {H264PROFILE_MAIN, VAProfileH264Main},
    {H264PROFILE_HIGH, VAProfileH264High},
    {VP8PROFILE_ANY, VAProfileVP8Version0_3},
    {VP9PROFILE_PROFILE0, VAProfileVP9Profile0},
    {VP9PROFILE_PROFILE2, VAProfileVP9Profile2},
    {AV1PROFILE_PROFILE_MAIN, VAProfileAV1Profile0},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    {HEVCPROFILE_MAIN, VAProfileHEVCMain},
    {HEVCPROFILE_MAIN_STILL_PICTURE, VAProfileHEVCMain},
    {HEVCPROFILE_MAIN10, VAProfileHEVCMain10},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  };
  auto it = kProfileMap.find(profile);
  return it != kProfileMap.end() ? std::make_optional<VAProfile>(it->second)
                                 : std::nullopt;
}

// Converts the given string to VAProfile
std::optional<VAProfile> StringToVAProfile(const std::string& va_profile) {
  const std::map<std::string, VAProfile> kStringToVAProfile = {
    {"VAProfileNone", VAProfileNone},
    {"VAProfileH264ConstrainedBaseline", VAProfileH264ConstrainedBaseline},
    // Even though it's deprecated, we leave VAProfileH264Baseline's
    // translation here to assert we never encounter it.
    {"VAProfileH264Baseline", VAProfileH264Baseline},
    {"VAProfileH264Main", VAProfileH264Main},
    {"VAProfileH264High", VAProfileH264High},
    {"VAProfileJPEGBaseline", VAProfileJPEGBaseline},
    {"VAProfileVP8Version0_3", VAProfileVP8Version0_3},
    {"VAProfileVP9Profile0", VAProfileVP9Profile0},
    {"VAProfileVP9Profile2", VAProfileVP9Profile2},
    {"VAProfileAV1Profile0", VAProfileAV1Profile0},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    {"VAProfileHEVCMain", VAProfileHEVCMain},
    {"VAProfileHEVCMain10", VAProfileHEVCMain10},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
    {"VAProfileProtected", VAProfileProtected},
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  };

  auto it = kStringToVAProfile.find(va_profile);
  return it != kStringToVAProfile.end()
             ? std::make_optional<VAProfile>(it->second)
             : std::nullopt;
}

// Converts the given string to VAEntrypoint
std::optional<VAEntrypoint> StringToVAEntrypoint(
    const std::string& va_entrypoint) {
  const std::map<std::string, VAEntrypoint> kStringToVAEntrypoint = {
    {"VAEntrypointVLD", VAEntrypointVLD},
    {"VAEntrypointEncSlice", VAEntrypointEncSlice},
    {"VAEntrypointEncPicture", VAEntrypointEncPicture},
    {"VAEntrypointEncSliceLP", VAEntrypointEncSliceLP},
    {"VAEntrypointVideoProc", VAEntrypointVideoProc},
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
    {"VAEntrypointProtectedContent", VAEntrypointProtectedContent},
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  };

  auto it = kStringToVAEntrypoint.find(va_entrypoint);
  return it != kStringToVAEntrypoint.end()
             ? std::make_optional<VAEntrypoint>(it->second)
             : std::nullopt;
}

unsigned int ToVaRTFormat(uint32_t va_fourcc) {
  switch (va_fourcc) {
    case VA_FOURCC_I420:
    case VA_FOURCC_NV12:
      return VA_RT_FORMAT_YUV420;
    case VA_FOURCC_YUY2:
      return VA_RT_FORMAT_YUV422;
    case VA_FOURCC_RGBA:
      return VA_RT_FORMAT_RGB32;
    case VA_FOURCC_P010:
      return VA_RT_FORMAT_YUV420_10;
  }
  return kInvalidVaRtFormat;
}

uint32_t ToVaFourcc(unsigned int va_rt_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return VA_FOURCC_NV12;
    case VA_RT_FORMAT_YUV420_10:
      return VA_FOURCC_P010;
  }
  return DRM_FORMAT_INVALID;
}

int ToGBMFormat(unsigned int va_rt_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return DRM_FORMAT_NV12;
    case VA_RT_FORMAT_YUV420_10:
      return DRM_FORMAT_P010;
  }
  return DRM_FORMAT_INVALID;
}

const std::string VARTFormatToString(unsigned int va_rt_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return "VA_RT_FORMAT_YUV420";
    case VA_RT_FORMAT_YUV420_10:
      return "VA_RT_FORMAT_YUV420_10";
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown VA_RT_FORMAT 0x" << std::hex << va_rt_format;
  return "Unknown VA_RT_FORMAT";
}

#define TOSTR(enumCase) \
  case enumCase:        \
    return #enumCase

const char* VAProfileToString(VAProfile profile) {
  // clang-format off
  switch (profile) {
    TOSTR(VAProfileNone);
    TOSTR(VAProfileMPEG2Simple);
    TOSTR(VAProfileMPEG2Main);
    TOSTR(VAProfileMPEG4Simple);
    TOSTR(VAProfileMPEG4AdvancedSimple);
    TOSTR(VAProfileMPEG4Main);
    case VAProfileH264Baseline:
      NOTREACHED_IN_MIGRATION() << "VAProfileH264Baseline is deprecated";
      return "Deprecated VAProfileH264Baseline";
    TOSTR(VAProfileH264Main);
    TOSTR(VAProfileH264High);
    TOSTR(VAProfileVC1Simple);
    TOSTR(VAProfileVC1Main);
    TOSTR(VAProfileVC1Advanced);
    TOSTR(VAProfileH263Baseline);
    TOSTR(VAProfileH264ConstrainedBaseline);
    TOSTR(VAProfileJPEGBaseline);
    TOSTR(VAProfileVP8Version0_3);
    TOSTR(VAProfileH264MultiviewHigh);
    TOSTR(VAProfileH264StereoHigh);
    TOSTR(VAProfileHEVCMain);
    TOSTR(VAProfileHEVCMain10);
    TOSTR(VAProfileVP9Profile0);
    TOSTR(VAProfileVP9Profile1);
    TOSTR(VAProfileVP9Profile2);
    TOSTR(VAProfileVP9Profile3);
    TOSTR(VAProfileHEVCMain12);
    TOSTR(VAProfileHEVCMain422_10);
    TOSTR(VAProfileHEVCMain422_12);
    TOSTR(VAProfileHEVCMain444);
    TOSTR(VAProfileHEVCMain444_10);
    TOSTR(VAProfileHEVCMain444_12);
    TOSTR(VAProfileHEVCSccMain);
    TOSTR(VAProfileHEVCSccMain10);
    TOSTR(VAProfileHEVCSccMain444);
    TOSTR(VAProfileAV1Profile0);
    TOSTR(VAProfileAV1Profile1);
    TOSTR(VAProfileHEVCSccMain444_10);
#if VA_MAJOR_VERSION >= 2 || VA_MINOR_VERSION >= 11
    TOSTR(VAProfileProtected);
#endif
#if VA_MAJOR_VERSION >= 2 || VA_MINOR_VERSION >= 18
    TOSTR(VAProfileH264High10);
#endif
#if VA_MAJOR_VERSION >= 2 || VA_MINOR_VERSION >= 22
    TOSTR(VAProfileVVCMain10);
    TOSTR(VAProfileVVCMultilayerMain10);
#endif
  }
  // clang-format on
  return "<unknown profile>";
}

// Returns true if the Display version is 14. CPU model ID's are referenced from
// the following file in the kernel source: arch/x86/include/asm/intel-family.h.
bool IsDisplayVer14() {
  constexpr int kMeteorLakeModelId = 0xAC;
  constexpr int kMeteorLake_LModelId = 0xAA;
  constexpr int kPentiumAndLaterFamily = 0x06;
  const base::CPU cpuid;
  return cpuid.family() == kPentiumAndLaterFamily &&
         (cpuid.model() == kMeteorLakeModelId ||
          cpuid.model() == kMeteorLake_LModelId);
}

}  // namespace

class VaapiTest : public testing::Test {
 public:
  VaapiTest() = default;
  ~VaapiTest() override = default;
};

std::map<VAProfile, std::vector<VAEntrypoint>> ParseVainfo(
    const std::string& output) {
  const std::vector<std::string> lines =
      base::SplitString(output, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_ALL);
  std::map<VAProfile, std::vector<VAEntrypoint>> info;
  for (const std::string& line : lines) {
    if (!base::StartsWith(line, "VAProfile",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    std::vector<std::string> res =
        base::SplitString(line, ":", base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_ALL);
    if (res.size() != 2) {
      LOG(ERROR) << "Unexpected line: " << line;
      continue;
    }

    auto va_profile = StringToVAProfile(res[0]);
    if (!va_profile)
      continue;
    auto va_entrypoint = StringToVAEntrypoint(res[1]);
    if (!va_entrypoint)
      continue;
    info[*va_profile].push_back(*va_entrypoint);
    DVLOG(3) << line;
  }
  return info;
}

std::string GetVaInfo(std::vector<std::string> argv) {
  int fds[2];
  PCHECK(pipe(fds) == 0);
  base::File read_pipe(fds[0]);
  base::ScopedFD write_pipe_fd(fds[1]);

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(write_pipe_fd.get(), STDOUT_FILENO);
  EXPECT_TRUE(LaunchProcess(argv, options).IsValid());
  write_pipe_fd.reset();

  char buf[262144] = {};
  int n = read_pipe.ReadAtCurrentPos(buf, sizeof(buf));
  PCHECK(n >= 0);
  EXPECT_LT(n, 262144);
  std::string output(buf, n);
  DVLOG(4) << output;
  return output;
}

std::map<VAProfile, std::vector<VAEntrypoint>> RetrieveVAInfoOutput() {
  std::vector<std::string> argv = {"vainfo"};
  std::string output = GetVaInfo(argv);
  return ParseVainfo(output);
}

TEST_F(VaapiTest, VaapiSandboxInitialization) {
  // Here we just test that the PreSandboxInitialization() in SetUp() worked
  // fine. Said initialization is buried in internal singletons, but we can
  // verify that at least the implementation type has been filled in.
  EXPECT_NE(VaapiWrapper::GetImplementationType(), VAImplementation::kInvalid);
}

// Commit [1] deprecated VAProfileH264Baseline from libva in 2017 (release
// 2.0.0). This test verifies that such profile is never seen in the lab.
// [1] https://github.com/intel/libva/commit/6f69256f8ccc9a73c0b196ab77ac69ab1f4f33c2
TEST_F(VaapiTest, VerifyNoVAProfileH264Baseline) {
  const auto va_info = RetrieveVAInfoOutput();
  EXPECT_FALSE(base::Contains(va_info, VAProfileH264Baseline));
}

// Verifies that every VAProfile from VaapiWrapper::GetSupportedDecodeProfiles()
// is indeed supported by the command line vainfo utility and by
// VaapiWrapper::IsDecodeSupported().
TEST_F(VaapiTest, GetSupportedDecodeProfiles) {
  const auto va_info = RetrieveVAInfoOutput();

  for (const auto& profile : VaapiWrapper::GetSupportedDecodeProfiles()) {
    const auto va_profile = ConvertToVAProfile(profile.profile);
    ASSERT_TRUE(va_profile.has_value());

    EXPECT_TRUE(base::Contains(va_info.at(*va_profile), VAEntrypointVLD))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
    EXPECT_TRUE(VaapiWrapper::IsDecodeSupported(*va_profile))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
  }
}

// Verifies that every VAProfile from VaapiWrapper::GetSupportedEncodeProfiles()
// is indeed supported by the command line vainfo utility.
TEST_F(VaapiTest, GetSupportedEncodeProfiles) {
  const auto va_info = RetrieveVAInfoOutput();

  for (const auto& profile : VaapiWrapper::GetSupportedEncodeProfiles()) {
    const auto va_profile = ConvertToVAProfile(profile.profile);
    ASSERT_TRUE(va_profile.has_value());
    constexpr VAProfile kSupportableVideoEncoderProfiles[] = {
        VAProfileH264ConstrainedBaseline,
        VAProfileH264Main,
        VAProfileH264High,
        VAProfileVP8Version0_3,
        VAProfileVP9Profile0,
        VAProfileAV1Profile0,
    };
    // Check if VaapiWrapper reports a profile that is not supported by
    // VaapiVideoEncodeAccelerator.
    ASSERT_TRUE(base::Contains(kSupportableVideoEncoderProfiles, va_profile));

    EXPECT_TRUE(base::Contains(va_info.at(*va_profile), VAEntrypointEncSlice) ||
                base::Contains(va_info.at(*va_profile), VAEntrypointEncSliceLP))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
  }
}

// Verifies that the resolutions of profiles for VBR and CBR are the same.
TEST_F(VaapiTest, VbrAndCbrResolutionsMatch) {
  struct ResolutionInfo {
    VaapiWrapper::CodecMode mode;
    gfx::Size min;
    gfx::Size max;
  };
  std::map<VAProfile, std::vector<ResolutionInfo>> supported_resolutions;
  for (const VaapiWrapper::CodecMode codec_mode :
       {VaapiWrapper::kEncodeConstantBitrate,
        VaapiWrapper::kEncodeConstantQuantizationParameter,
        VaapiWrapper::kEncodeVariableBitrate}) {
    const std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
            codec_mode);
    for (const auto& configuration : configurations) {
      const VAProfile va_profile = configuration.first;
      ResolutionInfo res_info{.mode = codec_mode};
      ASSERT_TRUE(VaapiWrapper::GetSupportedResolutions(
          va_profile, codec_mode, res_info.min, res_info.max))
          << " Failed get resolutions: "
          << "profile=" << va_profile << ", mode=" << codec_mode;

      supported_resolutions[va_profile].push_back(res_info);
    }
  }

  for (const auto& r : supported_resolutions) {
    const VAProfile va_profile = r.first;
    const auto& resolution_info = r.second;

    for (size_t i = 0; i < resolution_info.size(); ++i) {
      for (size_t j = i + 1; j < resolution_info.size(); ++j) {
        EXPECT_EQ(resolution_info[i].min, resolution_info[j].min)
            << " Minimum supported resolution mismatch for profile="
            << VAProfileToString(va_profile) << ": " << resolution_info[i].mode
            << " (" << resolution_info[i].min.ToString() << ") and "
            << resolution_info[j].mode << " ("
            << resolution_info[j].min.ToString() << ")";
        EXPECT_EQ(resolution_info[i].max, resolution_info[j].max)
            << " Maximum supported resolution mismatch for profile="
            << VAProfileToString(va_profile) << ": " << resolution_info[i].mode
            << " (" << resolution_info[i].max.ToString() << ") and "
            << resolution_info[j].mode << " ("
            << resolution_info[j].max.ToString() << ")";
      }
    }
  }
}

#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies that VAProfileProtected is indeed supported by the command line
// vainfo utility.
TEST_F(VaapiTest, VaapiProfileProtected) {
  VAImplementation impl = VaapiWrapper::GetImplementationType();
  // VAProfileProtected is only used in the Intel iHD implementation. AMD does
  // not need to support that profile (but should be the only other protected
  // content VAAPI implementation).
  if (impl == VAImplementation::kIntelIHD) {
    const auto va_info = RetrieveVAInfoOutput();

    EXPECT_TRUE(base::Contains(va_info.at(VAProfileProtected),
                               VAEntrypointProtectedContent))
        << ", va profile: " << vaProfileStr(VAProfileProtected);
  } else {
    EXPECT_EQ(impl, VAImplementation::kMesaGallium);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

// Verifies that if JPEG decoding and encoding are supported by VaapiWrapper,
// they are also supported by by the command line vainfo utility.
TEST_F(VaapiTest, VaapiProfilesJPEG) {
  const auto va_info = RetrieveVAInfoOutput();

  EXPECT_EQ(VaapiWrapper::IsDecodeSupported(VAProfileJPEGBaseline),
            base::Contains(va_info.at(VAProfileJPEGBaseline), VAEntrypointVLD));
  EXPECT_EQ(VaapiWrapper::IsJpegEncodeSupported(),
            base::Contains(va_info.at(VAProfileJPEGBaseline),
                           VAEntrypointEncPicture));
}

// Verifies that the default VAEntrypoint as per VaapiWrapper is indeed among
// the supported ones.
TEST_F(VaapiTest, DefaultEntrypointIsSupported) {
  for (size_t i = 0; i < VaapiWrapper::kCodecModeMax; ++i) {
    const auto wrapper_mode = static_cast<VaapiWrapper::CodecMode>(i);
    std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
            wrapper_mode);
    for (const auto& profile_and_entrypoints : configurations) {
      const VAEntrypoint default_entrypoint =
          VaapiWrapper::GetDefaultVaEntryPoint(wrapper_mode,
                                               profile_and_entrypoints.first);
      const auto& supported_entrypoints = profile_and_entrypoints.second;
      EXPECT_TRUE(base::Contains(supported_entrypoints, default_entrypoint))
          << "Default VAEntrypoint " << vaEntrypointStr(default_entrypoint)
          << " (VaapiWrapper mode = " << wrapper_mode
          << ") is not supported for "
          << vaProfileStr(profile_and_entrypoints.first);
    }
  }
}

// Verifies that VaapiWrapper::Create...() fails when called with an unsupported
// codec profile.
TEST_F(VaapiTest, UnsupportedVAProfile) {
  std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
      VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
          VaapiWrapper::kDecode);
  // H.263 decoding is NOT supported anywhere, but leave an ASSERT JIC.
  constexpr auto kUnsupportedVAProfile = VAProfileH263Baseline;
  ASSERT_FALSE(base::Contains(configurations, kUnsupportedVAProfile));

  auto wrapper_or_error =
      VaapiWrapper::Create(VaapiWrapper::kDecode, kUnsupportedVAProfile,
                           EncryptionScheme::kUnencrypted, base::DoNothing());
  ASSERT_FALSE(wrapper_or_error.has_value());
  ASSERT_EQ(wrapper_or_error.error(),
            DecoderStatus::Codes::kUnsupportedProfile);
}

// Verifies that VaapiWrapper::Create...() fails after the limit of created
// instances exceeds the threshold.
TEST_F(VaapiTest, TooManyDecoderInstances) {
  std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
      VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
          VaapiWrapper::kDecode);
  // H.264 decoding is currently supported everywhere, but leave an ASSERT.
  constexpr auto kVAProfile = VAProfileH264ConstrainedBaseline;
  ASSERT_TRUE(base::Contains(configurations, kVAProfile));

  const int kMaxNumOfInstances = VaapiWrapper::GetMaxNumDecoderInstances();
  std::vector<scoped_refptr<VaapiWrapper>> vaapi_wrappers(kMaxNumOfInstances);
  for (auto& wrapper : vaapi_wrappers) {
    auto wrapper_or_error =
        VaapiWrapper::Create(VaapiWrapper::kDecode, kVAProfile,
                             EncryptionScheme::kUnencrypted, base::DoNothing());
    ASSERT_TRUE(wrapper_or_error.has_value());
    wrapper = std::move(wrapper_or_error.value());
  }
  // Next one fails
  auto wrapper_or_error = VaapiWrapper::Create(VaapiWrapper::kDecode, kVAProfile,
                                     EncryptionScheme::kUnencrypted,
                                     base::DoNothing());
  ASSERT_FALSE(wrapper_or_error.has_value());
  ASSERT_EQ(wrapper_or_error.error(), DecoderStatus::Codes::kTooManyDecoders);
}

// Verifies that VaapiWrapper::Create...() fails when an EncryptionScheme is
// specified for a non-protected CodecMode.
TEST_F(VaapiTest, EncryptionSchemeNeedsCodecMode) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  GTEST_SKIP() << "This test only applies to Chrome Ash builds.";
#else
  std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
      VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
          VaapiWrapper::kDecode);
  // H.264 decoding is currently supported everywhere, but leave an ASSERT.
  constexpr auto kVAProfile = VAProfileH264ConstrainedBaseline;
  ASSERT_TRUE(base::Contains(configurations, kVAProfile));

  auto wrapper_or_error =
      VaapiWrapper::Create(VaapiWrapper::kDecode, kVAProfile,
                           EncryptionScheme::kCenc, base::DoNothing());
  ASSERT_FALSE(wrapper_or_error.has_value());
  ASSERT_EQ(wrapper_or_error.error(), DecoderStatus::Codes::kFailed);
#endif
}

// Verifies that VaapiWrapper::CreateContext() will queue up a buffer to set the
// encoder to its lowest quality setting if a given VAProfile and VAEntrypoint
// claims to support configuring it.
TEST_F(VaapiTest, LowQualityEncodingSetting) {
  // This test only applies to low powered Intel processors.
  constexpr int kPentiumAndLaterFamily = 0x06;
  const base::CPU cpuid;
  const bool is_core_y_processor =
      base::MatchPattern(cpuid.cpu_brand(), "Intel(R) Core(TM) *Y CPU*");
  const bool is_low_power_intel =
      cpuid.family() == kPentiumAndLaterFamily &&
      (base::Contains(cpuid.cpu_brand(), "Pentium") ||
       base::Contains(cpuid.cpu_brand(), "Celeron") || is_core_y_processor);
  if (!is_low_power_intel)
    GTEST_SKIP() << "Not an Intel low power processor";

  for (const auto& codec_mode :
       {VaapiWrapper::kEncodeConstantBitrate,
        VaapiWrapper::kEncodeConstantQuantizationParameter}) {
    std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
            codec_mode);

    for (const auto& profile_and_entrypoints : configurations) {
      const VAProfile va_profile = profile_and_entrypoints.first;
      scoped_refptr<VaapiWrapper> wrapper =
          VaapiWrapper::Create(VaapiWrapper::kEncodeConstantBitrate, va_profile,
                               EncryptionScheme::kUnencrypted,
                               base::DoNothing())
              .value_or(nullptr);

      // Depending on the GPU Gen, flags and policies, we may or may not utilize
      // all entrypoints (e.g. we might always want VAEntrypointEncSliceLP if
      // supported and enabled). Query VaapiWrapper's mandated entry point.
      const VAEntrypoint entrypoint =
          VaapiWrapper::GetDefaultVaEntryPoint(codec_mode, va_profile);
      ASSERT_TRUE(base::Contains(profile_and_entrypoints.second, entrypoint));

      VAConfigAttrib attrib{};
      attrib.type = VAConfigAttribEncQualityRange;
      {
        VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(wrapper->sequence_checker_);
        base::AutoLockMaybe auto_lock(wrapper->va_lock_.get());
        VAStatus va_res = vaGetConfigAttributes(
            wrapper->va_display_, va_profile, entrypoint, &attrib, 1);
        ASSERT_EQ(va_res, VA_STATUS_SUCCESS);
      }
      const auto quality_level = attrib.value;
      if (quality_level == VA_ATTRIB_NOT_SUPPORTED || quality_level <= 1u)
        continue;
      DLOG(INFO) << vaProfileStr(va_profile)
                 << " supports encoding quality setting, with max value "
                 << quality_level;

      // If we get here it means the |va_profile| and |entrypoint| support
      // the quality setting. We cannot inspect what the driver does with this
      // number (it could ignore it), so instead just make sure there's a
      // |pending_va_buffers_| that, when mapped, looks correct. That buffer
      // should be created by CreateContext().
      ASSERT_TRUE(wrapper->CreateContext(gfx::Size(640, 368)));
      VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(wrapper->sequence_checker_);
      ASSERT_EQ(wrapper->pending_va_buffers_.size(), 1u);
      {
        base::AutoLockMaybe auto_lock(wrapper->va_lock_.get());

        auto mapping = ScopedVABufferMapping::Create(
            wrapper->va_lock_, wrapper->va_display_,
            wrapper->pending_va_buffers_.front());
        ASSERT_TRUE(mapping);
        auto* const va_buffer =
            reinterpret_cast<VAEncMiscParameterBuffer*>(mapping->data());
        EXPECT_EQ(va_buffer->type, VAEncMiscParameterTypeQualityLevel);

        auto* const enc_quality =
            reinterpret_cast<VAEncMiscParameterBufferQualityLevel*>(
                va_buffer->data);
        EXPECT_EQ(enc_quality->quality_level, quality_level)
            << vaProfileStr(va_profile) << " " << vaEntrypointStr(entrypoint);
      }
    }
  }
}

// This test checks the supported SVC scalability mode.
TEST_F(VaapiTest, CheckSupportedSVCScalabilityModes) {
  const std::vector<SVCScalabilityMode> kSupportedL1T1 = {
      SVCScalabilityMode::kL1T1};
#if BUILDFLAG(IS_CHROMEOS)
  const std::vector<SVCScalabilityMode> kSupportedTemporalSVC = {
      SVCScalabilityMode::kL1T1, SVCScalabilityMode::kL1T2,
      SVCScalabilityMode::kL1T3};
  const std::vector<SVCScalabilityMode> kSupportedTemporalAndKeySVC = {
      SVCScalabilityMode::kL1T1,    SVCScalabilityMode::kL1T2,
      SVCScalabilityMode::kL1T3,    SVCScalabilityMode::kL2T2Key,
      SVCScalabilityMode::kL2T3Key, SVCScalabilityMode::kL3T2Key,
      SVCScalabilityMode::kL3T3Key, SVCScalabilityMode::kS2T1,
      SVCScalabilityMode::kS2T2,    SVCScalabilityMode::kS2T3,
      SVCScalabilityMode::kS3T1,    SVCScalabilityMode::kS3T2,
      SVCScalabilityMode::kS3T3};
#endif

  const auto scalability_modes_vp9_profile0 =
      VaapiWrapper::GetSupportedScalabilityModes(VP9PROFILE_PROFILE0,
                                                 VAProfileVP9Profile0);
#if BUILDFLAG(IS_CHROMEOS)
  const VAEntrypoint vp9_cqp_enc_va_entry_point =
      VaapiWrapper::GetDefaultVaEntryPoint(
          VaapiWrapper::kEncodeConstantQuantizationParameter,
          VAProfileVP9Profile0);
  if (vp9_cqp_enc_va_entry_point == VAEntrypointEncSliceLP ||
      vp9_cqp_enc_va_entry_point == VAEntrypointEncSlice) {
    EXPECT_EQ(scalability_modes_vp9_profile0, kSupportedTemporalAndKeySVC);
  } else {
    EXPECT_EQ(scalability_modes_vp9_profile0, kSupportedTemporalSVC);
  }
#else
  EXPECT_EQ(scalability_modes_vp9_profile0, kSupportedL1T1);
#endif

  const auto scalability_modes_vp9_profile2 =
      VaapiWrapper::GetSupportedScalabilityModes(VP9PROFILE_PROFILE2,
                                                 VAProfileVP9Profile2);
  EXPECT_EQ(scalability_modes_vp9_profile2, kSupportedL1T1);

  const auto scalability_modes_vp8 = VaapiWrapper::GetSupportedScalabilityModes(
      VP8PROFILE_ANY, VAProfileVP8Version0_3);
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(kVaapiVp8TemporalLayerHWEncoding)) {
    EXPECT_EQ(scalability_modes_vp8, kSupportedTemporalSVC);
  } else {
    EXPECT_EQ(scalability_modes_vp8, kSupportedL1T1);
  }
#else
  EXPECT_EQ(scalability_modes_vp8, kSupportedL1T1);
#endif

  const auto scalability_modes_h264_baseline =
      VaapiWrapper::GetSupportedScalabilityModes(
          H264PROFILE_BASELINE, VAProfileH264ConstrainedBaseline);
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(kVaapiH264TemporalLayerHWEncoding)) {
    EXPECT_EQ(scalability_modes_h264_baseline, kSupportedTemporalSVC);
  } else {
    EXPECT_EQ(scalability_modes_h264_baseline, kSupportedL1T1);
  }
#else
  EXPECT_EQ(scalability_modes_h264_baseline, kSupportedL1T1);
#endif
}

class VaapiVppTest
    : public VaapiTest,
      public testing::WithParamInterface<std::tuple<uint32_t, uint32_t>> {
 public:
  VaapiVppTest() = default;
  ~VaapiVppTest() override = default;

  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      std::stringstream ss;
      ss << FourccToString(std::get<0>(info.param)) << "_to_"
         << FourccToString(std::get<1>(info.param));
      return ss.str();
    }
  };
};

TEST_P(VaapiVppTest, BlitWithVAAllocatedSurfaces) {
  const uint32_t va_fourcc_in = std::get<0>(GetParam());
  const uint32_t va_fourcc_out = std::get<1>(GetParam());

  // TODO(b/187852384): enable the other two backends.
  if (VaapiWrapper::GetImplementationType() != VAImplementation::kIntelIHD)
    GTEST_SKIP() << "backend not supported";

  if (!VaapiWrapper::IsVppFormatSupported(va_fourcc_in) ||
      !VaapiWrapper::IsVppFormatSupported(va_fourcc_out)) {
    GTEST_SKIP() << FourccToString(va_fourcc_in) << " -> "
                 << FourccToString(va_fourcc_out) << " not supported";
  }
  constexpr gfx::Size kInputSize(640, 320);
  constexpr gfx::Size kOutputSize(320, 180);
  ASSERT_TRUE(VaapiWrapper::IsVppResolutionAllowed(kInputSize));
  ASSERT_TRUE(VaapiWrapper::IsVppResolutionAllowed(kOutputSize));

  auto wrapper =
      VaapiWrapper::Create(VaapiWrapper::kVideoProcess, VAProfileNone,
                           EncryptionScheme::kUnencrypted, base::DoNothing())
          .value_or(nullptr);
  ASSERT_TRUE(!!wrapper);
  // Size is unnecessary for a VPP context.
  ASSERT_TRUE(wrapper->CreateContext(gfx::Size()));

  const unsigned int va_rt_format_in = ToVaRTFormat(va_fourcc_in);
  ASSERT_NE(va_rt_format_in, kInvalidVaRtFormat);
  const unsigned int va_rt_format_out = ToVaRTFormat(va_fourcc_out);
  ASSERT_NE(va_rt_format_out, kInvalidVaRtFormat);

  auto scoped_surfaces = wrapper->CreateScopedVASurfaces(
      va_rt_format_in, kInputSize, {VaapiWrapper::SurfaceUsageHint::kGeneric},
      1u, /*visible_size=*/std::nullopt, /*va_fourcc=*/std::nullopt);
  ASSERT_FALSE(scoped_surfaces.empty());
  std::unique_ptr<ScopedVASurface> scoped_surface_in =
      std::move(scoped_surfaces[0]);

  scoped_surfaces = wrapper->CreateScopedVASurfaces(
      va_rt_format_out, kOutputSize, {VaapiWrapper::SurfaceUsageHint::kGeneric},
      1u, /*visible_size=*/std::nullopt, /*va_fourcc=*/std::nullopt);
  ASSERT_FALSE(scoped_surfaces.empty());
  std::unique_ptr<ScopedVASurface> scoped_surface_out =
      std::move(scoped_surfaces[0]);

  ASSERT_TRUE(wrapper->BlitSurface(
      scoped_surface_in->id(), kInputSize, scoped_surface_out->id(),
      kOutputSize, gfx::Rect(kInputSize), gfx::Rect(kOutputSize)));
  ASSERT_TRUE(wrapper->SyncSurface(scoped_surface_out->id()));
  wrapper->DestroyContext();
}

// TODO(b/187852384): Consider adding more VaapiVppTest cases, e.g. crops.

// Note: vaCreateSurfaces() uses the RT version of the Four CC, so we don't need
// to consider swizzlings, since they'll end up mapped to the same RT format.
constexpr uint32_t kVAFourCCs[] = {VA_FOURCC_I420, VA_FOURCC_YUY2,
                                   VA_FOURCC_RGBA, VA_FOURCC_P010};

INSTANTIATE_TEST_SUITE_P(,
                         VaapiVppTest,
                         ::testing::Combine(::testing::ValuesIn(kVAFourCCs),
                                            ::testing::ValuesIn(kVAFourCCs)),
                         VaapiVppTest::PrintToStringParamName());

class VaapiMinigbmTest
    : public VaapiTest,
      public testing::WithParamInterface<
          std::tuple<VAProfile, unsigned int /*va_rt_format*/, gfx::Size>> {
 public:
  VaapiMinigbmTest() = default;
  ~VaapiMinigbmTest() override = default;

  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      // Using here vaProfileStr(std::get<0>(info.param)) crashes the binary.
      // TODO(mcasas): investigate why and use it instead of codec%d.
      return base::StringPrintf(
          "%s__%s__%s", VAProfileToString(std::get<0>(info.param)),
          VARTFormatToString(std::get<1>(info.param)).c_str(),
          std::get<2>(info.param).ToString().c_str());
    }
  };
};

// This test allocates a VASurface (via VaapiWrapper) for the given VAProfile,
// VA RT Format and resolution (as per the test parameters). It then verifies
// that said VASurface's metadata (e.g. width, height, number of planes, pitch)
// are the same as those we would allocate via minigbm.
TEST_P(VaapiMinigbmTest, AllocateAndCompareWithMinigbm) {
  const VAProfile va_profile = std::get<0>(GetParam());
  const unsigned int va_rt_format = std::get<1>(GetParam());
  const gfx::Size resolution = std::get<2>(GetParam());

  // TODO(b/187852384): enable the other backends.
  const auto backend = VaapiWrapper::GetImplementationType();
  if (!(backend == VAImplementation::kIntelIHD ||
        backend == VAImplementation::kMesaGallium)) {
    GTEST_SKIP() << "backend not supported";
  }

  ASSERT_NE(va_rt_format, kInvalidVaRtFormat);
  if (!VaapiWrapper::IsDecodeSupported(va_profile))
    GTEST_SKIP() << vaProfileStr(va_profile) << " not supported.";

  if (!VaapiWrapper::IsDecodingSupportedForInternalFormat(va_profile,
                                                          va_rt_format)) {
    GTEST_SKIP() << VARTFormatToString(va_rt_format) << " not supported.";
  }
  // TODO(b/200817282): Fix high-bit depth formats on AMD Gallium impl.
  if (backend == VAImplementation::kMesaGallium &&
      va_rt_format == VA_RT_FORMAT_YUV420_10) {
    GTEST_SKIP() << vaProfileStr(va_profile) << " fails on AMD, skipping.";
  }

  gfx::Size minimum_supported_size;
  gfx::Size maximum_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetSupportedResolutions(
      va_profile, VaapiWrapper::CodecMode::kDecode, minimum_supported_size,
      maximum_supported_size));
  if (resolution.width() < minimum_supported_size.width() ||
      resolution.height() < minimum_supported_size.height() ||
      resolution.width() > maximum_supported_size.width() ||
      resolution.height() > maximum_supported_size.height()) {
    GTEST_SKIP() << resolution.ToString()
                 << " not supported (min: " << minimum_supported_size.ToString()
                 << ", max: " << maximum_supported_size.ToString() << ")";
  }

  auto wrapper =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile,
                           EncryptionScheme::kUnencrypted, base::DoNothing())
          .value_or(nullptr);
  ASSERT_TRUE(!!wrapper);
  ASSERT_TRUE(wrapper->CreateContext(resolution));

  auto scoped_surfaces = wrapper->CreateScopedVASurfaces(
      va_rt_format, resolution, {VaapiWrapper::SurfaceUsageHint::kVideoDecoder},
      1u,
      /*visible_size=*/std::nullopt, /*va_fourcc=*/std::nullopt);
  ASSERT_FALSE(scoped_surfaces.empty());
  const auto scoped_va_surface = std::move(scoped_surfaces[0]);
  wrapper->DestroyContext();

  ASSERT_TRUE(scoped_va_surface->IsValid());
  EXPECT_EQ(scoped_va_surface->format(), va_rt_format);

  // Request the underlying DRM metadata for |scoped_va_surface|.
  VADRMPRIMESurfaceDescriptor va_descriptor{};
  {
    VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(wrapper->sequence_checker_);
    base::AutoLockMaybe auto_lock(wrapper->va_lock_.get());
    VAStatus va_res =
        vaSyncSurface(wrapper->va_display_, scoped_va_surface->id());
    ASSERT_EQ(va_res, VA_STATUS_SUCCESS);
    va_res = vaExportSurfaceHandle(
        wrapper->va_display_, scoped_va_surface->id(),
        VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
        &va_descriptor);
    ASSERT_EQ(va_res, VA_STATUS_SUCCESS);
  }

  // Verify some expected properties of the allocated VASurface. We expect one
  // or two |object|s, with a number of |layers| of the same |pitch|.
  EXPECT_EQ(scoped_va_surface->size(),
            gfx::Size(base::checked_cast<int>(va_descriptor.width),
                      base::checked_cast<int>(va_descriptor.height)));

  const auto va_fourcc = ToVaFourcc(va_rt_format);
  ASSERT_NE(va_fourcc, base::checked_cast<unsigned int>(DRM_FORMAT_INVALID));
  EXPECT_EQ(va_descriptor.fourcc, va_fourcc)
      << FourccToString(va_descriptor.fourcc)
      << " != " << FourccToString(va_fourcc);
  EXPECT_THAT(va_descriptor.num_objects, ::testing::AnyOf(1, 2));
  // TODO(mcasas): consider comparing |size| with a better estimate of the
  // |scoped_va_surface| memory footprint (e.g. including planes and format).
  EXPECT_GE(va_descriptor.objects[0].size,
            base::checked_cast<uint32_t>(scoped_va_surface->size().GetArea()));
  if (va_descriptor.num_objects == 2) {
    const int uv_width = (scoped_va_surface->size().width() + 1) / 2;
    const int uv_height = (scoped_va_surface->size().height() + 1) / 2;
    EXPECT_GE(va_descriptor.objects[1].size,
              base::checked_cast<uint32_t>(2 * uv_width * uv_height));
  }

  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(wrapper->sequence_checker_);
  base::AutoLockMaybe auto_lock(wrapper->va_lock_.get());
  const std::string va_vendor_string
         = vaQueryVendorString(wrapper->va_display_);
  uint64_t expected_drm_modifier = DRM_FORMAT_MOD_LINEAR;

  if (backend == VAImplementation::kIntelIHD) {
    expected_drm_modifier =
        IsDisplayVer14() ? I915_FORMAT_MOD_4_TILED : I915_FORMAT_MOD_Y_TILED;
  } else if (backend == VAImplementation::kMesaGallium) {
    if (va_vendor_string.find("stoney") != std::string::npos) {
      expected_drm_modifier = DRM_FORMAT_MOD_INVALID;
    }
  }
  EXPECT_EQ(va_descriptor.objects[0].drm_format_modifier,
            expected_drm_modifier);
  // TODO(mcasas): |num_layers| actually depends on |va_descriptor.va_fourcc|.
  EXPECT_EQ(va_descriptor.num_layers, 2u);
  for (uint32_t i = 0; i < va_descriptor.num_layers; ++i) {
    EXPECT_EQ(va_descriptor.layers[i].num_planes, 1u);
    const uint32_t expected_object_index =
        (va_descriptor.num_objects == 1) ? 0 : i;
    EXPECT_EQ(va_descriptor.layers[i].object_index[0], expected_object_index);

    DVLOG(2) << "plane " << i
             << ", pitch: " << va_descriptor.layers[i].pitch[0];
    // Luma and chroma planes have different |pitch| expectations.
    // TODO(mcasas): consider bitdepth for pitch lower thresholds.
    if (i == 0) {
      EXPECT_GE(
          va_descriptor.layers[i].pitch[0],
          base::checked_cast<uint32_t>(scoped_va_surface->size().width()));
    } else {
      const auto expected_rounded_up_pitch =
          base::bits::AlignUpDeprecatedDoNotUse(
              scoped_va_surface->size().width(), 2);
      EXPECT_GE(va_descriptor.layers[i].pitch[0],
                base::checked_cast<uint32_t>(expected_rounded_up_pitch));
    }
  }

  // Now open minigbm pointing to the DRM primary node, allocate a gbm_bo, and
  // compare its width/height/stride/etc with the |va_descriptor|s.
  constexpr char kPrimaryNodeFilePattern[] = "/dev/dri/card%d";
  struct gbm_device* gbm = nullptr;
  base::File drm_fd;
  // This loop ends on either the first card that does not exist or the first
  // primary node that is not vgem.
  for (int i = 0;; i++) {
    base::FilePath dev_path(FILE_PATH_LITERAL(
        base::StringPrintf(kPrimaryNodeFilePattern, i).c_str()));
    drm_fd =
        base::File(dev_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
    ASSERT_TRUE(drm_fd.IsValid());
    // Skip the virtual graphics memory manager device.
    drmVersionPtr version = drmGetVersion(drm_fd.GetPlatformFile());
    if (!version)
      continue;
    std::string version_name(
        version->name,
        base::checked_cast<std::string::size_type>(version->name_len));
    drmFreeVersion(version);
    if (base::EqualsCaseInsensitiveASCII(version_name, "vgem"))
      continue;

    gbm = gbm_create_device(drm_fd.GetPlatformFile());
    break;
  }

  ASSERT_TRUE(gbm);

  const auto gbm_format = ToGBMFormat(va_rt_format);
  ASSERT_NE(gbm_format, DRM_FORMAT_INVALID);
  const auto bo_use_flags = GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_DECODER;
  struct gbm_bo* bo =
      gbm_bo_create(gbm, resolution.width(), resolution.height(), gbm_format,
                    bo_use_flags | GBM_BO_USE_SCANOUT);
  if (!bo) {
    // Try again without the scanout flag. This reproduces Chrome's behaviour.
    bo = gbm_bo_create(gbm, resolution.width(), resolution.height(), gbm_format,
                       bo_use_flags);
  }
  ASSERT_TRUE(bo);
  EXPECT_EQ(scoped_va_surface->size(),
            gfx::Size(base::checked_cast<int>(gbm_bo_get_width(bo)),
                      base::checked_cast<int>(gbm_bo_get_height(bo))));

  const int bo_num_planes = gbm_bo_get_plane_count(bo);
  ASSERT_EQ(va_descriptor.num_layers,
            base::checked_cast<uint32_t>(bo_num_planes));
  for (int i = 0; i < bo_num_planes; ++i) {
    EXPECT_EQ(va_descriptor.layers[i].pitch[0],
              gbm_bo_get_stride_for_plane(bo, i));
  }

  // TODO(mcasas): consider comparing |va_descriptor.objects[0].size| with |bo|s
  // size (as returned by lseek()ing it).

  gbm_bo_destroy(bo);
  gbm_device_destroy(gbm);
}

constexpr VAProfile kVACodecProfiles[] = {
    VAProfileVP8Version0_3, VAProfileH264ConstrainedBaseline,
    VAProfileVP9Profile0,   VAProfileVP9Profile2,
    VAProfileAV1Profile0,   VAProfileJPEGBaseline};
constexpr uint32_t kVARTFormatsForGBM[] = {VA_RT_FORMAT_YUV420,
                                           VA_RT_FORMAT_YUV420_10};
constexpr gfx::Size kResolutions[] = {
    // clang-format off
    gfx::Size(127, 127),
    gfx::Size(128, 128),
    gfx::Size(129, 129),
    gfx::Size(320, 180),
    gfx::Size(320, 240),  // QVGA
    gfx::Size(323, 243),
    gfx::Size(480, 320),  // 3/4 VGA
    gfx::Size(640, 360),  // VGA
    gfx::Size(640, 480),
    gfx::Size(1280, 720)};
// clang-format on

INSTANTIATE_TEST_SUITE_P(
    ,
    VaapiMinigbmTest,
    ::testing::Combine(::testing::ValuesIn(kVACodecProfiles),
                       ::testing::ValuesIn(kVARTFormatsForGBM),
                       ::testing::ValuesIn(kResolutions)),
    VaapiMinigbmTest::PrintToStringParamName());

}  // namespace media

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // PreSandboxInitialization() loads and opens the driver, queries its
  // capabilities and fills in the VASupportedProfiles.
  media::VaapiWrapper::PreSandboxInitialization();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
