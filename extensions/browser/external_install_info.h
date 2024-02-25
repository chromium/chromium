// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTERNAL_INSTALL_INFO_H_
#define EXTENSIONS_BROWSER_EXTERNAL_INSTALL_INFO_H_

#include "base/files/file_path.h"
#include "base/version.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {

// Holds information about an external extension install from an external
// provider.
struct ExternalInstallInfo {
  ExternalInstallInfo(const ExtensionId& extension_id,
                      int creation_flags,
                      bool mark_acknowledged);
  ExternalInstallInfo(const ExternalInstallInfo&) = delete;
  ExternalInstallInfo& operator=(const ExternalInstallInfo&) = delete;
  ExternalInstallInfo(ExternalInstallInfo&& other);
  virtual ~ExternalInstallInfo() {}

  ExtensionId extension_id;
  int creation_flags;
  bool mark_acknowledged;
};

struct ExternalInstallInfoFile : public ExternalInstallInfo {
  ExternalInstallInfoFile(const ExtensionId& extension_id,
                          const base::Version& version,
                          const base::FilePath& path,
                          mojom::ManifestLocation crx_location,
                          int creation_flags,
                          bool mark_acknowledged,
                          bool install_immediately);
  ExternalInstallInfoFile(ExternalInstallInfoFile&& other);
  ~ExternalInstallInfoFile() override;

  base::Version version;
  base::FilePath path;
  mojom::ManifestLocation crx_location;
  bool install_immediately;
};

struct ExternalInstallInfoUpdateUrl : public ExternalInstallInfo {
  ExternalInstallInfoUpdateUrl(const ExtensionId& extension_id,
                               const std::string& install_parameter,
                               GURL update_url,
                               mojom::ManifestLocation download_location,
                               int creation_flags,
                               bool mark_acknowledged);
  ExternalInstallInfoUpdateUrl(ExternalInstallInfoUpdateUrl&& other);
  ~ExternalInstallInfoUpdateUrl() override;

  std::string install_parameter;
  GURL update_url;
  mojom::ManifestLocation download_location;
};

}  // namespace extensions
#endif  // EXTENSIONS_BROWSER_EXTERNAL_INSTALL_INFO_H_
