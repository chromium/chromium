// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {

bool WebUIIOSController::OverrideHandleWebUIIOSMessage(
    const GURL& source_url,
    base::StringPiece message) {
  return false;
}

}  // namespace web
