// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_
#define COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_

namespace webapps {

enum class UninstallResultCode {
  kSuccess,
  kNoAppToUninstall,
  kCancelled,
  kError,
};
}

#endif  // COMPONENTS_WEBAPPS_BROWSER_UNINSTALL_RESULT_CODE_H_
