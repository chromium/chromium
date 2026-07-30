// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_UPDATE_CHECK_AND_PREPARE_RESULT_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_UPDATE_CHECK_AND_PREPARE_RESULT_H_

#include <iosfwd>
#include <string>

#include "base/types/expected.h"

namespace web_app {

enum class IwaUpdateCheckAndPrepareSuccess {
  kNoUpdateFound,
  kUpdateAlreadyPending,
  kPinnedVersionUpdateFoundAndSavedInDatabase,  // Update to pinned version
                                                // was successful. This type
                                                // of update can happen only
                                                // once, right after the app
                                                // is pinned. After that, no
                                                // update should happen.
  kDowngradeVersionFoundAndSavedInDatabase,
  kUpdateFoundAndSavedInDatabase,
  kUpdateFound
};

enum class IwaUpdateCheckAndPrepareError {
  // Update Manifest errors
  kUpdateManifestDownloadFailed,
  kUpdateManifestInvalidJson,
  kUpdateManifestInvalidManifest,
  kUpdateManifestNoApplicableVersion,
  kIwaNotInstalled,

  // Version pinning errors
  kPinnedVersionNotFoundInUpdateManifest,

  // Version downgrade errors
  kDowngradetNotAllowed,

  // Signed Web Bundle download errors
  kDownloadPathCreationFailed,
  kBundleDownloadError,

  // Update dry run errors
  kUpdateDryRunFailed,

  kSystemShutdown,
};

using IwaUpdateCheckAndPrepareResult =
    base::expected<IwaUpdateCheckAndPrepareSuccess,
                   IwaUpdateCheckAndPrepareError>;

std::string IwaUpdateCheckAndPrepareSuccessToString(
    IwaUpdateCheckAndPrepareSuccess success);
std::string IwaUpdateCheckAndPrepareErrorToString(
    IwaUpdateCheckAndPrepareError error);

std::ostream& operator<<(std::ostream& os,
                         const IwaUpdateCheckAndPrepareSuccess& success);
std::ostream& operator<<(std::ostream& os,
                         const IwaUpdateCheckAndPrepareError& error);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_UPDATE_CHECK_AND_PREPARE_RESULT_H_
