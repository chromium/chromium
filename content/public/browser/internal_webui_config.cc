// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/internal_webui_config.h"

#include <set>

#include "base/containers/contains.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace content {

namespace {

using InternalWebUIHostSet = std::set<std::string_view>;

InternalWebUIHostSet& GetInternalWebUIHostSet() {
  static base::NoDestructor<InternalWebUIHostSet> s_host_set;
  return *s_host_set;
}

}  // namespace

InternalWebUIConfig::InternalWebUIConfig(std::string_view host)
    : WebUIConfig(content::kChromeUIScheme, host) {
  GetInternalWebUIHostSet().insert(this->host());
}

InternalWebUIConfig::~InternalWebUIConfig() {
  GetInternalWebUIHostSet().erase(this->host());
}

bool IsInternalWebUI(const GURL& url) {
  return GetInternalWebUIHostSet().contains(url.host());
}

}  // namespace content
