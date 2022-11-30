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
      pkp_bypassed(false),
      ct_policy_compliance(net::ct::CTPolicyCompliance::
                               CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE) {}

SSLStatus::SSLStatus(const net::SSLInfo& ssl_info)
    : initialized(true),
      certificate(ssl_info.cert),
      cert_status(ssl_info.cert_status),
      key_exchange_group(ssl_info.key_exchange_group),
      peer_signature_algorithm(ssl_info.peer_signature_algorithm),
      connection_status(ssl_info.connection_status),
      content_status(NORMAL_CONTENT),
      pkp_bypassed(ssl_info.pkp_bypassed),
      ct_policy_compliance(ssl_info.ct_policy_compliance) {}

SSLStatus::SSLStatus(const SSLStatus& other)
    : initialized(other.initialized),
      certificate(other.certificate),
      cert_status(other.cert_status),
      key_exchange_group(other.key_exchange_group),
      peer_signature_algorithm(other.peer_signature_algorithm),
      connection_status(other.connection_status),
      content_status(other.content_status),
      pkp_bypassed(other.pkp_bypassed),
      ct_policy_compliance(other.ct_policy_compliance) {}

SSLStatus& SSLStatus::operator=(SSLStatus other) {
  initialized = other.initialized;
  certificate = other.certificate;
  cert_status = other.cert_status;
  key_exchange_group = other.key_exchange_group;
  peer_signature_algorithm = other.peer_signature_algorithm;
  connection_status = other.connection_status;
  content_status = other.content_status;
  pkp_bypassed = other.pkp_bypassed;
  ct_policy_compliance = other.ct_policy_compliance;
  return *this;
}

SSLStatus::~SSLStatus() {}

}  // namespace content
