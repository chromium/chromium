// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webui_config.h"

#include <string_view>

namespace content {

WebUIConfig::WebUIConfig(std::string_view scheme, std::string_view host)
    : scheme_(scheme), host_(host) {}

WebUIConfig::~WebUIConfig() = default;

bool WebUIConfig::IsWebUIEnabled(BrowserContext* browser_context) {
  return true;
}

bool WebUIConfig::ShouldHandleURL(const GURL& url) {
  return true;
}

}  // namespace content
