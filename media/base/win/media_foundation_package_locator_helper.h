// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MEDIA_FOUNDATION_PACKAGE_LOCATOR_HELPER_H_
#define MEDIA_BASE_WIN_MEDIA_FOUNDATION_PACKAGE_LOCATOR_HELPER_H_

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "media/base/media_export.h"

namespace media {

enum class MediaFoundationCodecPackage {
  kAV1 = 0,
  kHEVC,
  kVP9,
  kDolbyVision,
  kAC4,
  kEAC3
};

// Locate Media Foundation based Codec Pack install paths by using Win32
// AppModel APIs.
MEDIA_EXPORT std::vector<base::FilePath> MediaFoundationPackageInstallPaths(
    const std::vector<std::wstring_view>& package_family_names,
    std::wstring_view decoder_lib_name,
    MediaFoundationCodecPackage codec_package);

}  // namespace media

#endif  // MEDIA_BASE_WIN_MEDIA_FOUNDATION_PACKAGE_LOCATOR_HELPER_H_
