// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_controller.h"

#include "content/browser/webui/web_ui_managed_interface.h"
#include "url/gurl.h"

namespace content {

WebUIController::WebUIController(WebUI* web_ui) : web_ui_(web_ui) {}

WebUIController::~WebUIController() {
  RemoveWebUIManagedInterfaces(this);
}

bool WebUIController::OverrideHandleWebUIMessage(const GURL& source_url,
                                                 const std::string& message,
                                                 const base::ListValue& args) {
  return false;
}

WebUIController::Type WebUIController::GetType() {
  return nullptr;
}

WebUIController::TrustPolicy WebUIController::GetTrustPolicy() {
  return TrustPolicy::kTrusted;
}

bool WebUIController::IsJavascriptErrorReportingEnabled() {
  return true;
}

}  // namespace content
