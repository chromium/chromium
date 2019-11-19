// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/security/ssl_status.h"

namespace web {

SSLStatus::SSLStatus()
    : security_style(SECURITY_STYLE_UNKNOWN),
      cert_status(0),
      content_status(NORMAL_CONTENT) {}

SSLStatus::SSLStatus(const SSLStatus& other)
    : security_style(other.security_style),
      certificate(other.certificate),
      cert_status(other.cert_status),
      content_status(other.content_status),
      cert_status_host(other.cert_status_host),
      user_data(other.user_data ? other.user_data->Clone() : nullptr) {}

SSLStatus& SSLStatus::operator=(SSLStatus other) {
  security_style = other.security_style;
  certificate = other.certificate;
  cert_status = other.cert_status;
  content_status = other.content_status;
  cert_status_host = other.cert_status_host;
  user_data = other.user_data ? other.user_data->Clone() : nullptr;
  return *this;
}

SSLStatus::~SSLStatus() {}

}  // namespace web
