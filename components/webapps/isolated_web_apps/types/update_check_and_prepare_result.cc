// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/types/update_check_and_prepare_result.h"

#include <ostream>
#include <string>

namespace web_app {

std::string IwaUpdateCheckAndPrepareSuccessToString(
    IwaUpdateCheckAndPrepareSuccess success) {
  switch (success) {
    case IwaUpdateCheckAndPrepareSuccess::kNoUpdateFound:
      return "Success::kNoUpdateFound";
    case IwaUpdateCheckAndPrepareSuccess::kUpdateAlreadyPending:
      return "Success::kUpdateAlreadyPending";
    case IwaUpdateCheckAndPrepareSuccess::
        kPinnedVersionUpdateFoundAndSavedInDatabase:
      return "Success::kPinnedVersionUpdateFoundAndSavedInDatabase";
    case IwaUpdateCheckAndPrepareSuccess::
        kDowngradeVersionFoundAndSavedInDatabase:
      return "Success::kDowngradeVersionFoundAndSavedInDatabase";
    case IwaUpdateCheckAndPrepareSuccess::kUpdateFoundAndSavedInDatabase:
      return "Success::kUpdateFoundAndDryRunSuccessful";
    case IwaUpdateCheckAndPrepareSuccess::kUpdateFound:
      return "Success::kUpdateFound";
  }
}

std::string IwaUpdateCheckAndPrepareErrorToString(
    IwaUpdateCheckAndPrepareError error) {
  switch (error) {
    case IwaUpdateCheckAndPrepareError::kUpdateManifestDownloadFailed:
      return "Failed to download update manifest.";
    case IwaUpdateCheckAndPrepareError::kUpdateManifestInvalidJson:
      return "Update manifest contains invalid JSON.";
    case IwaUpdateCheckAndPrepareError::kUpdateManifestInvalidManifest:
      return "Invalid update manifest format.";
    case IwaUpdateCheckAndPrepareError::kUpdateManifestNoApplicableVersion:
      return "No applicable version found in update manifest.";
    case IwaUpdateCheckAndPrepareError::kIwaNotInstalled:
      return "App not found.";
    case IwaUpdateCheckAndPrepareError::kPinnedVersionNotFoundInUpdateManifest:
      return "Pinned version not found in update manifest.";
    case IwaUpdateCheckAndPrepareError::kDowngradeNotAllowed:
      return "Version downgrade is not allowed.";
    case IwaUpdateCheckAndPrepareError::kDownloadPathCreationFailed:
      return "Failed to create download path.";
    case IwaUpdateCheckAndPrepareError::kBundleDownloadError:
      return "Failed to download web bundle.";
    case IwaUpdateCheckAndPrepareError::kUpdateDryRunFailed:
      return "Update dry run failed.";
    case IwaUpdateCheckAndPrepareError::kSystemShutdown:
      return "Operation aborted.";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const IwaUpdateCheckAndPrepareSuccess& success) {
  return os << IwaUpdateCheckAndPrepareSuccessToString(success);
}

std::ostream& operator<<(std::ostream& os,
                         const IwaUpdateCheckAndPrepareError& error) {
  return os << IwaUpdateCheckAndPrepareErrorToString(error);
}

}  // namespace web_app
