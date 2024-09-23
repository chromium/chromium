// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/media_foundation_package_runtime_locator.h"

#include <windows.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "media/base/win/media_foundation_package_locator_helper.h"

namespace media {

namespace {

constexpr wchar_t kMfPackageFamilyName_HEVC[] =
    L"Microsoft.HEVCVideoExtension_8wekyb3d8bbwe";
constexpr wchar_t kMfPackageFamilyName_HEVC_2[] =
    L"Microsoft.HEVCVideoExtensions_8wekyb3d8bbwe";
constexpr wchar_t kMfPackageFamilyName_DolbyVision[] =
    L"DolbyLaboratories.DolbyVisionAccess_rz1tebttyb220";
constexpr wchar_t kMfPackageFamilyName_DolbyVision_2[] =
    L"DolbyLaboratories.DolbyVisionHDR_rz1tebttyb220";
constexpr wchar_t kMfPackageFamilyName_AC4[] =
    L"DolbyLaboratories.DolbyAC4DecoderOEM_rz1tebttyb220";
constexpr wchar_t kMfPackageFamilyName_EAC3[] =
    L"DolbyLaboratories.DolbyDigitalPlusDecoderOEM_rz1tebttyb220";
constexpr wchar_t kMfLibraryName_AC4[] = L"DolbyAc4DecMft.dll";
constexpr wchar_t kMfLibraryName_EAC3[] = L"DolbyDDPDecMft.dll";

constexpr MediaFoundationCodecPackage kMfCodecPackages[] = {
    MediaFoundationCodecPackage::kEAC3, MediaFoundationCodecPackage::kAC4,
    MediaFoundationCodecPackage::kHEVC,
    MediaFoundationCodecPackage::kDolbyVision};

std::wstring GetMfCodecPackageName(MediaFoundationCodecPackage codec_package) {
  switch (codec_package) {
    case MediaFoundationCodecPackage::kAV1:
      return L"kAV1";
    case MediaFoundationCodecPackage::kHEVC:
      return L"kHEVC";
    case MediaFoundationCodecPackage::kVP9:
      return L"kVP9";
    case MediaFoundationCodecPackage::kDolbyVision:
      return L"kDolbyVision";
    case MediaFoundationCodecPackage::kAC4:
      return L"kAC4";
    case MediaFoundationCodecPackage::kEAC3:
      return L"kEAC3";
  }
}

std::vector<std::wstring_view> GetMfCodecPackageFamilyNames(
    MediaFoundationCodecPackage codec_package) {
  std::vector<std::wstring_view> package_family_names;
  switch (codec_package) {
    case MediaFoundationCodecPackage::kHEVC:
      package_family_names.push_back(kMfPackageFamilyName_HEVC);
      package_family_names.push_back(kMfPackageFamilyName_HEVC_2);
      break;
    case MediaFoundationCodecPackage::kDolbyVision:
      package_family_names.push_back(kMfPackageFamilyName_DolbyVision);
      package_family_names.push_back(kMfPackageFamilyName_DolbyVision_2);
      break;
    case MediaFoundationCodecPackage::kAC4:
      package_family_names.push_back(kMfPackageFamilyName_AC4);
      break;
    case MediaFoundationCodecPackage::kEAC3:
      package_family_names.push_back(kMfPackageFamilyName_EAC3);
      break;
    default:
      break;
  }
  return package_family_names;
}

std::wstring GetMfCodecPackageLibraryName(
    MediaFoundationCodecPackage codec_package) {
  switch (codec_package) {
    case MediaFoundationCodecPackage::kAC4:
      return kMfLibraryName_AC4;
    case MediaFoundationCodecPackage::kEAC3:
      return kMfLibraryName_EAC3;
    default:
      return L"";
  }
}

struct DllModuleInfo {
  std::vector<base::FilePath> Paths;
  std::optional<bool> Loaded;
};

// This helper class, based on media_foundation_package_locator_helper.h, offers
// two primary functionalities:
//   1. Detection of the existence of specific MediaFoundation codec packages on
//   the user's device. This capability can be utilized by the W3C Media
//   Capability checking API, including methods such as HTMLMediaElement:
//   canPlayType(), MediaSource: isTypeSupported(), and
//   mediaCapabilities.decodingInfo().
//   2. Loading codec libraries from codec packages into process memory.
//   Typically, the codec library is loaded into the Chromium GPU process before
//   it is sandboxed, aiming to resolve 'access denied' issues.
// It provides a static method to obtain a single instance of
// MediaFoundationPackageRuntimeLocator on-demand, which persists in process
// memory until the termination of the process.
class MediaFoundationPackageRuntimeLocator {
 public:
  static MediaFoundationPackageRuntimeLocator& GetInstance();
  MediaFoundationPackageRuntimeLocator(
      const MediaFoundationPackageRuntimeLocator&) = delete;
  MediaFoundationPackageRuntimeLocator& operator=(
      const MediaFoundationPackageRuntimeLocator&) = delete;
  bool FoundModule(MediaFoundationCodecPackage codec) const;
  bool LoadModule(MediaFoundationCodecPackage codec);

 private:
  friend class base::NoDestructor<MediaFoundationPackageRuntimeLocator>;
  MediaFoundationPackageRuntimeLocator();
  ~MediaFoundationPackageRuntimeLocator() = default;
  std::map<MediaFoundationCodecPackage, DllModuleInfo> mf_dll_module_;
};

// static method to get a single MediaFoundationPackageRuntimeLocator
// instance on-demand.
MediaFoundationPackageRuntimeLocator&
MediaFoundationPackageRuntimeLocator::GetInstance() {
  static base::NoDestructor<MediaFoundationPackageRuntimeLocator> instance;
  return *instance;
}

MediaFoundationPackageRuntimeLocator::MediaFoundationPackageRuntimeLocator() {
  DVLOG(1) << __func__;
  for (auto& codec_package : kMfCodecPackages) {
    auto package_names = GetMfCodecPackageFamilyNames(codec_package);
    auto package_library = GetMfCodecPackageLibraryName(codec_package);
    std::vector<base::FilePath> paths = MediaFoundationPackageInstallPaths(
        package_names, package_library, codec_package);
    if (paths.empty()) {
      DVLOG(2) << __func__ << ": MediaFoundation codec package ("
               << GetMfCodecPackageName(codec_package) << ") is not installed.";
      continue;
    }

    mf_dll_module_[codec_package] = DllModuleInfo{std::move(paths)};
  }
}

bool MediaFoundationPackageRuntimeLocator::FoundModule(
    MediaFoundationCodecPackage codec_package) const {
  auto result = mf_dll_module_.find(codec_package);
  if (result == mf_dll_module_.cend()) {
    DVLOG(2) << __func__ << ": Can not find codec package ("
             << GetMfCodecPackageName(codec_package) << ")";
    return false;
  }

  auto& module_info = result->second;
  if (module_info.Paths.empty()) {
    DVLOG(2) << __func__ << ": Found codec package ("
             << GetMfCodecPackageName(codec_package)
             << "), but it's library path is empty";
    return false;
  }

  if (module_info.Loaded.has_value() && !module_info.Loaded.value()) {
    DVLOG(2) << __func__ << ": Found codec package ("
             << GetMfCodecPackageName(codec_package)
             << "), but load it's library module failed";
    return false;
  }

  return true;
}

bool MediaFoundationPackageRuntimeLocator::LoadModule(
    MediaFoundationCodecPackage codec_package) {
  auto result = mf_dll_module_.find(codec_package);
  if (result == mf_dll_module_.cend()) {
    DVLOG(2) << __func__ << ": Can not find codec package ("
             << GetMfCodecPackageName(codec_package) << ")";
    return false;
  }

  auto& module_info = result->second;
  if (module_info.Loaded.has_value()) {
    return module_info.Loaded.value();
  }

  if (module_info.Paths.empty()) {
    DVLOG(2) << __func__ << ": Found codec package ("
             << GetMfCodecPackageName(codec_package)
             << "), but it's library path is empty";
    return false;
  }

  for (auto& path : module_info.Paths) {
    if (LoadNativeLibrary(path, nullptr)) {
      // Intentionally don't call UnloadNativeLibrary, to ensure it
      // stays loaded
      module_info.Loaded = true;
      DVLOG(3) << __func__ << ": MediaFoundation package "
               << GetMfCodecPackageName(codec_package) << " loaded";
      return true;
    }
  }
  module_info.Loaded = false;
  return false;
}

std::optional<MediaFoundationCodecPackage>
AudioCodecToMediaFoundationCodecPackage(AudioCodec codec) {
  std::optional<MediaFoundationCodecPackage> package_type{std::nullopt};
  switch (codec) {
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
      package_type = MediaFoundationCodecPackage::kEAC3;
      break;
    case AudioCodec::kAC4:
      package_type = MediaFoundationCodecPackage::kAC4;
      break;
    default:
      DVLOG(3) << "Audio codec " << GetCodecName(codec)
               << " has no MediaFoundation package.";
      break;
  }
  return package_type;
}

}  // namespace

bool LoadMediaFoundationPackageDecoder(AudioCodec codec) {
  auto codec_package = AudioCodecToMediaFoundationCodecPackage(codec);
  return codec_package.has_value()
             ? MediaFoundationPackageRuntimeLocator::GetInstance().LoadModule(
                   codec_package.value())
             : false;
}

bool FindMediaFoundationPackageDecoder(AudioCodec codec) {
  auto codec_package = AudioCodecToMediaFoundationCodecPackage(codec);
  return codec_package.has_value()
             ? MediaFoundationPackageRuntimeLocator::GetInstance().FoundModule(
                   codec_package.value())
             : false;
}

}  // namespace media
