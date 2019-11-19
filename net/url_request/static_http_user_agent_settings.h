// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_STATIC_HTTP_USER_AGENT_SETTINGS_H_
#define NET_URL_REQUEST_STATIC_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/net_export.h"

namespace net {

// An implementation of |HttpUserAgentSettings| that provides configured
// values for the HTTP Accept-Language and User-Agent headers.
class NET_EXPORT StaticHttpUserAgentSettings : public HttpUserAgentSettings {
 public:
  StaticHttpUserAgentSettings(const std::string& accept_language,
                              const std::string& user_agent);
  ~StaticHttpUserAgentSettings() override;

  void set_accept_language(const std::string& new_accept_language) {
    accept_language_ = new_accept_language;
  }

  // HttpUserAgentSettings implementation
  std::string GetAcceptLanguage() const override;
  std::string GetUserAgent() const override;

 private:
  std::string accept_language_;
  const std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(StaticHttpUserAgentSettings);
};

}  // namespace net

#endif  // NET_URL_REQUEST_STATIC_HTTP_USER_AGENT_SETTINGS_H_
