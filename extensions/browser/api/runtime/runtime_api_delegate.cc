// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/runtime/runtime_api_delegate.h"

namespace extensions {

RuntimeAPIDelegate::UpdateCheckResult::UpdateCheckResult(
    const api::runtime::RequestUpdateCheckStatus& status,
    const std::string& version)
    : status(status), version(version) {}

bool RuntimeAPIDelegate::OpenOptionsPage(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  return false;
}

int RuntimeAPIDelegate::GetDeveloperToolsWindowId(
    content::WebContents* developer_tools_web_contents) {
  return -1;
}

}  // namespace extensions
