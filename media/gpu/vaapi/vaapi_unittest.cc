// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include <unistd.h>
#include <map>
#include <vector>

#include <va/va.h>
#include <va/va_str.h>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/media_switches.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/media_buildflags.h"

namespace media {
namespace {

base::Optional<VAProfile> ConvertToVAProfile(VideoCodecProfile profile) {
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
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    {HEVCPROFILE_MAIN, VAProfileHEVCMain},
    {HEVCPROFILE_MAIN10, VAProfileHEVCMain10},
#endif
  };
  auto it = kProfileMap.find(profile);
  return it != kProfileMap.end() ? base::make_optional<VAProfile>(it->second)
                                 : base::nullopt;
}

// Converts the given string to VAProfile
base::Optional<VAProfile> StringToVAProfile(const std::string& va_profile) {
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
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    {"VAProfileHEVCMain", VAProfileHEVCMain},
    {"VAProfileHEVCMain10", VAProfileHEVCMain10},
#endif
  };

  auto it = kStringToVAProfile.find(va_profile);
  return it != kStringToVAProfile.end()
             ? base::make_optional<VAProfile>(it->second)
             : base::nullopt;
}

// Converts the given string to VAProfile
base::Optional<VAEntrypoint> StringToVAEntrypoint(
    const std::string& va_entrypoint) {
  const std::map<std::string, VAEntrypoint> kStringToVAEntrypoint = {
      {"VAEntrypointVLD", VAEntrypointVLD},
      {"VAEntrypointEncSlice", VAEntrypointEncSlice},
      {"VAEntrypointEncPicture", VAEntrypointEncPicture},
      {"VAEntrypointEncSliceLP", VAEntrypointEncSliceLP},
      {"VAEntrypointVideoProc", VAEntrypointVideoProc}};

  auto it = kStringToVAEntrypoint.find(va_entrypoint);
  return it != kStringToVAEntrypoint.end()
             ? base::make_optional<VAEntrypoint>(it->second)
             : base::nullopt;
}

std::unique_ptr<base::test::ScopedFeatureList> CreateScopedFeatureList() {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /*enabled_features=*/{media::kVaapiAV1Decoder},
      /*disabled_features=*/{});
  return scoped_feature_list;
}
}  // namespace

class VaapiTest : public testing::Test {
 public:
  VaapiTest() : scoped_feature_list_(CreateScopedFeatureList()) {}
  ~VaapiTest() override = default;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
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

std::map<VAProfile, std::vector<VAEntrypoint>> RetrieveVAInfoOutput() {
  int fds[2];
  PCHECK(pipe(fds) == 0);
  base::File read_pipe(fds[0]);
  base::ScopedFD write_pipe_fd(fds[1]);

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(write_pipe_fd.get(), STDOUT_FILENO);
  std::vector<std::string> argv = {"vainfo"};
  EXPECT_TRUE(LaunchProcess(argv, options).IsValid());
  write_pipe_fd.reset();

  char buf[4096] = {};
  int n = read_pipe.ReadAtCurrentPos(buf, sizeof(buf));
  PCHECK(n >= 0);
  EXPECT_LT(n, 4096);
  std::string output(buf, n);
  DVLOG(4) << output;
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

  for (const auto& profile : VaapiWrapper::GetSupportedDecodeProfiles(
           gpu::GpuDriverBugWorkarounds())) {
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

    EXPECT_TRUE(base::Contains(va_info.at(*va_profile), VAEntrypointEncSlice) ||
                base::Contains(va_info.at(*va_profile), VAEntrypointEncSliceLP))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
  }
}

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

  std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
      VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
          VaapiWrapper::kEncode);

  for (const auto& codec_mode :
       {VaapiWrapper::kEncode,
        VaapiWrapper::kEncodeConstantQuantizationParameter}) {
    std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
            codec_mode);

    for (const auto& profile_and_entrypoints : configurations) {
      const VAProfile va_profile = profile_and_entrypoints.first;
      scoped_refptr<VaapiWrapper> wrapper = VaapiWrapper::Create(
          VaapiWrapper::kEncode, va_profile, EncryptionScheme::kUnencrypted,
          base::DoNothing());

      // Depending on the GPU Gen, flags and policies, we may or may not utilize
      // all entrypoints (e.g. we might always want VAEntrypointEncSliceLP if
      // supported and enabled). Query VaapiWrapper's mandated entry point.
      const VAEntrypoint entrypoint =
          VaapiWrapper::GetDefaultVaEntryPoint(codec_mode, va_profile);
      ASSERT_TRUE(base::Contains(profile_and_entrypoints.second, entrypoint));

      VAConfigAttrib attrib{};
      attrib.type = VAConfigAttribEncQualityRange;
      {
        base::AutoLock auto_lock(*wrapper->va_lock_);
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
      ASSERT_EQ(wrapper->pending_va_buffers_.size(), 1u);
      {
        base::AutoLock auto_lock(*wrapper->va_lock_);
        ScopedVABufferMapping mapping(wrapper->va_lock_, wrapper->va_display_,
                                      wrapper->pending_va_buffers_.front());
        ASSERT_TRUE(mapping.IsValid());

        auto* const va_buffer =
            reinterpret_cast<VAEncMiscParameterBuffer*>(mapping.data());
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
}  // namespace media

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  {
    // Enables/Disables features during PreSandboxInitialization(). We have to
    // destruct ScopedFeatureList after it because base::TestSuite::Run()
    // creates a ScopedFeatureList and multiple concurrent ScopedFeatureLists
    // are not allowed.
    auto scoped_feature_list = media::CreateScopedFeatureList();
    // PreSandboxInitialization() loads and opens the driver, queries its
    // capabilities and fills in the VASupportedProfiles.
    media::VaapiWrapper::PreSandboxInitialization();
  }

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
