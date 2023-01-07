// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/static_http_user_agent_settings.h"

namespace net {

StaticHttpUserAgentSettings::StaticHttpUserAgentSettings(
    const std::string& accept_language,
    const std::string& user_agent)
    : accept_language_(accept_language),
      user_agent_(user_agent) {
}

StaticHttpUserAgentSettings::~StaticHttpUserAgentSettings() = default;

std::string StaticHttpUserAgentSettings::GetAcceptLanguage() const {
  return accept_language_;
}

std::string StaticHttpUserAgentSettings::GetUserAgent() const {
  return user_agent_;
}

}  // namespace net
