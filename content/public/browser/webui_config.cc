// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webui_config.h"

namespace content {

WebUIConfig::WebUIConfig(base::StringPiece scheme, base::StringPiece host)
    : scheme_(scheme), host_(host) {}

WebUIConfig::~WebUIConfig() = default;

bool WebUIConfig::IsWebUIEnabled(BrowserContext* browser_context) {
  return true;
}

}  // namespace content
