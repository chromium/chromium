// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/runtime/runtime_api_delegate.h"

namespace extensions {

RuntimeAPIDelegate::UpdateCheckResult::UpdateCheckResult(
    bool success,
    const std::string& response,
    const std::string& version)
    : success(success), response(response), version(version) {
}

bool RuntimeAPIDelegate::OpenOptionsPage(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  return false;
}

}  // namespace extensions
