// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_
#define COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_

#include <iosfwd>

namespace webapps {

enum class UninstallResultCode {
  // The app was uninstalled since there is no other install source or url.
  kAppRemoved,
  kNoAppToUninstall,
  kCancelled,
  kError,
  kShutdown,
  // The specified install source was removed, but others remain, so the app was
  // not uninstalled.
  kInstallSourceRemoved,
  // The specified install url was removed, but others remain, so the app was
  // not uninstalled.
  kInstallUrlRemoved,
};

bool UninstallSucceeded(UninstallResultCode code);

std::ostream& operator<<(std::ostream& os, UninstallResultCode code);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_
