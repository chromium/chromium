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
      return "Error::kUpdateManifestDownloadFailed";
    case IwaUpdateCheckAndPrepareError::kUpdateManifestInvalidJson:
      return "Error::kUpdateManifestInvalidJson";
    case IwaUpdateCheckAndPrepareError::kUpdateManifestInvalidManifest:
      return "Error::kUpdateManifestInvalidManifest";
    case IwaUpdateCheckAndPrepareError::kUpdateManifestNoApplicableVersion:
      return "Error::kUpdateManifestNoApplicableVersion";
    case IwaUpdateCheckAndPrepareError::kIwaNotInstalled:
      return "Error::kIwaNotInstalled";
    case IwaUpdateCheckAndPrepareError::kPinnedVersionNotFoundInUpdateManifest:
      return "Error::kPinnedVersionNotFoundInUpdateManifest";
    case IwaUpdateCheckAndPrepareError::kDowngradetNotAllowed:
      return "Error::kDowngradetNotAllowed";
    case IwaUpdateCheckAndPrepareError::kBundleDownloadError:
      return "Error::kBundleDownloadError";
    case IwaUpdateCheckAndPrepareError::kDownloadPathCreationFailed:
      return "Error::kDownloadPathCreationFailed";
    case IwaUpdateCheckAndPrepareError::kUpdateDryRunFailed:
      return "Error::kUpdateDryRunFailed";
    case IwaUpdateCheckAndPrepareError::kSystemShutdown:
      return "Error::kSystemShutdown";
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
