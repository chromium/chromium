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
#include "base/strings/string_split.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

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
