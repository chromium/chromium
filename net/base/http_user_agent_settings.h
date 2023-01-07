// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HTTP_USER_AGENT_SETTINGS_H_
#define NET_BASE_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

// The interface used by HTTP jobs to retrieve HTTP Accept-Language
// and User-Agent header values.
class NET_EXPORT HttpUserAgentSettings {
 public:
  HttpUserAgentSettings() = default;
  HttpUserAgentSettings(const HttpUserAgentSettings&) = delete;
  HttpUserAgentSettings& operator=(const HttpUserAgentSettings&) = delete;
  virtual ~HttpUserAgentSettings() = default;

  // Gets the value of 'Accept-Language' header field.
  virtual std::string GetAcceptLanguage() const = 0;

  // Gets the UA string.
  virtual std::string GetUserAgent() const = 0;
};

}  // namespace net

#endif  // NET_BASE_HTTP_USER_AGENT_SETTINGS_H_
