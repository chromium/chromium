// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ssl_status.h"

#include "net/ssl/ssl_info.h"

namespace content {

SSLStatus::SSLStatus()
    : initialized(false),
      cert_status(0),
      key_exchange_group(0),
      peer_signature_algorithm(0),
      connection_status(0),
      content_status(NORMAL_CONTENT),
      pkp_bypassed(false) {}

SSLStatus::SSLStatus(const net::SSLInfo& ssl_info)
    : initialized(true),
      certificate(ssl_info.cert),
      cert_status(ssl_info.cert_status),
      key_exchange_group(ssl_info.key_exchange_group),
      peer_signature_algorithm(ssl_info.peer_signature_algorithm),
      connection_status(ssl_info.connection_status),
      content_status(NORMAL_CONTENT),
      pkp_bypassed(ssl_info.pkp_bypassed) {}

SSLStatus::SSLStatus(const SSLStatus& other) = default;
SSLStatus& SSLStatus::operator=(const SSLStatus& other) = default;

SSLStatus::~SSLStatus() = default;

}  // namespace content
