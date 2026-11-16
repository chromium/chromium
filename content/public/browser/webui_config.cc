// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webui_config.h"

#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

WebUIConfig::WebUIConfig(std::string_view scheme,
                         std::string_view host,
                         std::optional<std::string_view> process_lock_host)
    : scheme_(scheme), host_(host), process_lock_host_(process_lock_host) {
  if (process_lock_host_.has_value()) {
    // Only trusted `chrome://` WebUIs can share a `chrome://` process lock.
    CHECK_EQ(scheme_, kChromeUIScheme);
    // The host must form a valid `chrome://<host>/` URL on its own.
    GURL lock_url(base::StrCat(
        {kChromeUIScheme, url::kStandardSchemeSeparator, *process_lock_host_}));
    CHECK(lock_url.is_valid());
    CHECK_EQ(lock_url.host(), *process_lock_host_);
  }
}

WebUIConfig::~WebUIConfig() = default;

bool WebUIConfig::IsWebUIEnabled(BrowserContext* browser_context) {
  return true;
}

bool WebUIConfig::ShouldHandleURL(const GURL& url) {
  return true;
}

bool WebUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild() const {
  return false;
}

bool WebUIConfig::ShouldKeepVisibleUntilFirstVisuallyNonEmptyPaint() {
  return false;
}

bool WebUIConfig::SupportsInProcessResourceLoadingV2() const {
  return false;
}

}  // namespace content
