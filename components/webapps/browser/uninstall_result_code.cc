// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/uninstall_result_code.h"

#include <ostream>

namespace webapps {

bool UninstallSucceeded(UninstallResultCode code) {
  switch (code) {
    case UninstallResultCode::kSuccess:
    case UninstallResultCode::kNoAppToUninstall:
      return true;
    case UninstallResultCode::kCancelled:
    case UninstallResultCode::kError:
    case UninstallResultCode::kShutdown:
      return false;
  }
}

std::ostream& operator<<(std::ostream& os, UninstallResultCode code) {
  switch (code) {
    case UninstallResultCode::kSuccess:
      return os << "kSuccess";
    case UninstallResultCode::kNoAppToUninstall:
      return os << "kNoAppToUninstall";
    case UninstallResultCode::kCancelled:
      return os << "kCancelled";
    case UninstallResultCode::kError:
      return os << "kError";
    case UninstallResultCode::kShutdown:
      return os << "kShutdown";
  }
}

}  // namespace webapps
