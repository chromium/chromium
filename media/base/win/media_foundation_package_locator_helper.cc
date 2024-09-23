// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/media_foundation_package_locator_helper.h"

#include <windows.h>

#include <appmodel.h>

#include "base/logging.h"

namespace media {

std::vector<base::FilePath> MediaFoundationPackageInstallPaths(
    const std::vector<std::wstring_view>& package_family_names,
    std::wstring_view decoder_lib_name,
    MediaFoundationCodecPackage codec_package) {
  std::vector<base::FilePath> package_paths;  // Collected paths to return.
  for (const auto& package_family_name : package_family_names) {
    // Loop for each package family name.
    DVLOG(2) << __func__ << ": Use package_family_name=" << package_family_name;

    uint32_t package_count = 0;
    uint32_t buffer_size = 0;
    long rc =
        GetPackagesByPackageFamily(package_family_name.data(), &package_count,
                                   nullptr, &buffer_size, nullptr);
    if ((rc != ERROR_SUCCESS && rc != ERROR_INSUFFICIENT_BUFFER) ||
        package_count == 0) {
      DVLOG(2) << package_family_name
               << " decoder package family is not installed. rc=" << rc;
      continue;
    }

    DVLOG(2) << __func__ << ":" << package_family_name << " is installed.";
    // Allocate required sizes to get all the package fullnames.
    std::vector<wchar_t> buffer(buffer_size);
    std::vector<wchar_t*> package_full_names(package_count);
    rc = GetPackagesByPackageFamily(package_family_name.data(), &package_count,
                                    package_full_names.data(), &buffer_size,
                                    buffer.data());
    if (package_count == 0) {
      DLOG(WARNING) << "Cannot find " << package_family_name << ". rc=" << rc;
      return package_paths;
    }

    wchar_t package_path[MAX_PATH];
    // Get all the package paths for each package full name.
    for (const wchar_t* package_full_name : package_full_names) {
      DVLOG(2) << __func__ << ": package_full_name=" << package_full_name;
      uint32_t package_path_len = std::size(package_path);
      rc = GetPackagePathByFullName(package_full_name, &package_path_len,
                                    package_path);
      if (rc != ERROR_SUCCESS) {
        DLOG(WARNING) << "Cannot find " << package_full_name << ". rc=" << rc;
        package_paths.clear();
        return package_paths;
      }

      // Dolby places their MFT inside a 'MFT' folder.
      if (codec_package == MediaFoundationCodecPackage::kEAC3 ||
          codec_package == MediaFoundationCodecPackage::kAC4) {
        package_paths.emplace_back(base::FilePath(package_path).Append(L"MFT"));
      } else {
        package_paths.emplace_back(package_path);
      }
    }
  }

  if (package_paths.empty()) {
    DVLOG(2) << __func__ << ": empty package_paths.";
    return package_paths;
  }

  // AV1 1.1.50332.0 or later does not have the 'build' folder in the path.
  if (codec_package == MediaFoundationCodecPackage::kAV1) {
    std::vector<base::FilePath> legacy_av1_package_paths;
    for (const auto& package_path : package_paths) {
      legacy_av1_package_paths.emplace_back(package_path.Append(L"build"));
    }
    package_paths.insert(package_paths.end(), legacy_av1_package_paths.begin(),
                         legacy_av1_package_paths.end());
  }

  // Append the path to the library based on binary architecture.
  for (auto& package_path : package_paths) {
    constexpr wchar_t kArch[] =
#ifdef _M_AMD64
        L"x64";
#elif _M_IX86
        L"x86";
#elif _M_ARM64
        L"arm64";
#else
#error Unsupported processor type.
#endif
    package_path = package_path.Append(kArch);
    package_path = package_path.Append(decoder_lib_name);
    DVLOG(2) << __func__ << ": package_path=" << package_path.value();
  }
  return package_paths;
}

}  // namespace media
