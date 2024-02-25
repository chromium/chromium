// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/external_install_info.h"
#include "extensions/common/extension_id.h"

namespace extensions {

ExternalInstallInfo::ExternalInstallInfo(const ExtensionId& extension_id,
                                         int creation_flags,
                                         bool mark_acknowledged)
    : extension_id(extension_id),
      creation_flags(creation_flags),
      mark_acknowledged(mark_acknowledged) {}
ExternalInstallInfo::ExternalInstallInfo(ExternalInstallInfo&& other) = default;

ExternalInstallInfoFile::ExternalInstallInfoFile(
    const ExtensionId& extension_id,
    const base::Version& version,
    const base::FilePath& path,
    mojom::ManifestLocation crx_location,
    int creation_flags,
    bool mark_acknowledged,
    bool install_immediately)
    : ExternalInstallInfo(extension_id, creation_flags, mark_acknowledged),
      version(version),
      path(path),
      crx_location(crx_location),
      install_immediately(install_immediately) {}
ExternalInstallInfoFile::ExternalInstallInfoFile(
    ExternalInstallInfoFile&& other) = default;

ExternalInstallInfoFile::~ExternalInstallInfoFile() = default;

ExternalInstallInfoUpdateUrl::ExternalInstallInfoUpdateUrl(
    const ExtensionId& extension_id,
    const std::string& install_parameter,
    GURL update_url,
    mojom::ManifestLocation download_location,
    int creation_flags,
    bool mark_acknowledged)
    : ExternalInstallInfo(extension_id, creation_flags, mark_acknowledged),
      install_parameter(install_parameter),
      update_url(std::move(update_url)),
      download_location(download_location) {}
ExternalInstallInfoUpdateUrl::ExternalInstallInfoUpdateUrl(
    ExternalInstallInfoUpdateUrl&& other) = default;

ExternalInstallInfoUpdateUrl::~ExternalInstallInfoUpdateUrl() = default;

}  // namespace extensions
