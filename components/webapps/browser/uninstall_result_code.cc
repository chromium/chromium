// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/uninstall_result_code.h"

#include <string>

namespace webapps {

std::string ConvertUninstallResultCodeToString(UninstallResultCode code) {
  switch (code) {
    case UninstallResultCode::kSuccess:
      return "Success";
    case UninstallResultCode::kNoAppToUninstall:
      return "No App found for uninstall";
    case UninstallResultCode::kCancelled:
      return "Uninstall cancelled";
    case UninstallResultCode::kError:
      return "Error";
  }
}

}  // namespace webapps
