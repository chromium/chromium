// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_controller.h"

namespace content {

bool WebUIController::OverrideHandleWebUIMessage(const GURL& source_url,
                                                 const std::string& message,
                                                 const base::ListValue& args) {
  return false;
}

WebUIController::Type WebUIController::GetType() {
  return nullptr;
}

bool WebUIController::IsJavascriptErrorReportingEnabled() {
  return true;
}

}  // namespace content
