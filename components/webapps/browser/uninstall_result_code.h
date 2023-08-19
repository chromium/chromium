// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_
#define COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_

#include <string>

namespace webapps {

enum class UninstallResultCode {
  kSuccess,
  kNoAppToUninstall,
  kCancelled,
  kError,
  kShutdown,
};

bool UninstallSucceeded(UninstallResultCode code);

std::string ConvertUninstallResultCodeToString(UninstallResultCode code);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_
