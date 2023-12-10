// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfapi.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/win/scoped_co_mem.h"
#include "media/base/test_helpers.h"
#include "media/base/win/media_foundation_package_locator_helper.h"
#include "media/base/win/mf_initializer.h"

namespace media {

namespace {

using VideoCodecMap = base::flat_map<VideoCodec, GUID>;

const VideoCodecMap& GetVideoCodecsMap() {
  static const base::NoDestructor<VideoCodecMap> AllVideoCodecsMap(
      {{VideoCodec::kVP9, MFVideoFormat_VP90},
       {VideoCodec::kHEVC, MFVideoFormat_HEVC},
       {VideoCodec::kAV1, MFVideoFormat_AV1}});
  return *AllVideoCodecsMap;
}

bool CanMfDecodeVideoCodec(VideoCodec codec) {
  auto codecs = GetVideoCodecsMap();
  MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Video, codecs[codec]};
  base::win::ScopedCoMem<IMFActivate*> imf_activates;
  uint32_t count = 0;
  if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                       MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT |
                           MFT_ENUM_FLAG_HARDWARE,
                       &input_type, /*output_type=*/nullptr, &imf_activates,
                       &count))) {
    return false;
  }

  for (uint32_t i = 0; i < count; ++i) {
    imf_activates[i]->Release();
  }
  if (count == 0) {
    DLOG(INFO) << "No MFT for " << media::GetCodecName(codec);
    return false;
  }
  return true;
}

}  // namespace

class MediaFoundationPackageLocatorTest : public testing::Test {
 public:
  MediaFoundationPackageLocatorTest() = default;
  ~MediaFoundationPackageLocatorTest() override = default;

 protected:
  void SetUp() override {
    // We would like to use `MFTEnumEx()` in the test.
    ASSERT_TRUE(InitializeMediaFoundation());
  }

  void AddPackageFamilyName(const wchar_t* package_family_name) {
    media_foundation_package_family_names_.emplace_back(package_family_name);
  }

  std::vector<base::FilePath> GetMediaFoundationPackageInstallPaths(
      const std::wstring_view& decoder_lib_name,
      MediaFoundationCodecPackage codec_package) {
    return MediaFoundationPackageInstallPaths(
        media_foundation_package_family_names_, decoder_lib_name,
        codec_package);
  }

  void VerifyMfCodecPaths(std::vector<base::FilePath>& codec_paths) {
    ASSERT_FALSE(codec_paths.empty());
    // Verify the MF Codec Pack DLL module exists.
    bool mf_codec_dll_module_found = false;
    for (const auto& package_path : codec_paths) {
      DVLOG(2) << __func__ << ": package_path=" << package_path.value();
      if (base::PathExists(package_path)) {
        mf_codec_dll_module_found = true;
        break;
      }
    }
    ASSERT_TRUE(mf_codec_dll_module_found);
  }

  std::vector<std::wstring_view> media_foundation_package_family_names_;
};

TEST_F(MediaFoundationPackageLocatorTest, VP9) {
  AddPackageFamilyName(L"Microsoft.VP9VideoExtensions_8wekyb3d8bbwe");
  std::vector<base::FilePath> paths = GetMediaFoundationPackageInstallPaths(
      L"msvp9dec_store.dll", media::MediaFoundationCodecPackage::kVP9);

  if (CanMfDecodeVideoCodec(VideoCodec::kVP9)) {
    DVLOG(2) << __func__ << ": MF VP9 installed";
    VerifyMfCodecPaths(paths);
  } else {
    ASSERT_TRUE(paths.empty());
  }
}

TEST_F(MediaFoundationPackageLocatorTest, AV1) {
  AddPackageFamilyName(L"Microsoft.AV1VideoExtension_8wekyb3d8bbwe");
  std::vector<base::FilePath> paths = GetMediaFoundationPackageInstallPaths(
      L"av1decodermft_store.dll", media::MediaFoundationCodecPackage::kAV1);

  if (CanMfDecodeVideoCodec(VideoCodec::kAV1)) {
    DVLOG(2) << __func__ << ": MF AV1 installed";
    VerifyMfCodecPaths(paths);
  } else {
    ASSERT_TRUE(paths.empty());
  }
}

TEST_F(MediaFoundationPackageLocatorTest, HEVC) {
  AddPackageFamilyName(L"Microsoft.HEVCVideoExtension_8wekyb3d8bbwe");
  AddPackageFamilyName(L"Microsoft.HEVCVideoExtensions_8wekyb3d8bbwe");  // OEM.
  std::vector<base::FilePath> paths = GetMediaFoundationPackageInstallPaths(
      L"hevcdecoder_store.dll", media::MediaFoundationCodecPackage::kHEVC);

  if (CanMfDecodeVideoCodec(VideoCodec::kHEVC)) {
    DVLOG(2) << __func__ << ": MF HEVC installed";
    VerifyMfCodecPaths(paths);
  } else {
    ASSERT_TRUE(paths.empty());
  }
}

}  // namespace media
