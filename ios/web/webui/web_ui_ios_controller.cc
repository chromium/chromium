// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {

bool WebUIIOSController::OverrideHandleWebUIIOSMessage(
    const GURL& source_url,
    const std::string& message,
    const base::ListValue& args) {
  return false;
}

}  // namespace web
