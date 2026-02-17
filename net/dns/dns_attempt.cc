// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_attempt.h"

#include <stdint.h>

#include <string>

#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

Error DnsAttempt::FailureRcodeToNetError(int rcode) {
  DCHECK_NE(dns_protocol::kRcodeNOERROR, rcode);
  switch (rcode) {
    case dns_protocol::kRcodeFORMERR:
      return ERR_DNS_FORMAT_ERROR;
    case dns_protocol::kRcodeSERVFAIL:
      return ERR_DNS_SERVER_FAILURE;
    case dns_protocol::kRcodeNXDOMAIN:
      return ERR_NAME_NOT_RESOLVED;
    case dns_protocol::kRcodeNOTIMP:
      return ERR_DNS_NOT_IMPLEMENTED;
    case dns_protocol::kRcodeREFUSED:
      return ERR_DNS_REFUSED;
    default:
      return ERR_DNS_OTHER_FAILURE;
  }
}

base::DictValue DnsAttempt::NetLogStartParams(const std::string& hostname,
                                              uint16_t qtype) {
  base::DictValue dict;
  dict.Set("hostname", hostname);
  dict.Set("query_type", qtype);
  return dict;
}

DnsAttempt::DnsAttempt(size_t server_index) : server_index_(server_index) {}

// Returns the index of the destination server within DnsConfig::nameservers
// (or DnsConfig::dns_over_https_servers for secure transactions).
size_t DnsAttempt::server_index() const {
  return server_index_;
}

// Returns a Value representing the received response, along with a reference
// to the NetLog source source of the UDP socket used.  The request must have
// completed before this is called.
base::DictValue DnsAttempt::NetLogResponseParams(
    NetLogCaptureMode capture_mode) const {
  base::DictValue dict;

  if (GetResponse()) {
    DCHECK(GetResponse()->IsValid());
    dict.Set("rcode", GetResponse()->rcode());
    dict.Set("answer_count", static_cast<int>(GetResponse()->answer_count()));
    dict.Set("additional_answer_count",
             static_cast<int>(GetResponse()->additional_answer_count()));
  }

  GetSocketNetLog().source().AddToEventParameters(dict);

  if (capture_mode == NetLogCaptureMode::kEverything) {
    dict.Set("response_buffer", GetRawResponseBufferForLog());
  }

  return dict;
}

const net::NetworkTrafficAnnotationTag DnsAttempt::kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dns_transaction", R"(
      semantics {
        sender: "DNS Transaction"
        description:
          "DNS Transaction implements a stub DNS resolver as defined in RFC "
          "1034."
        trigger:
          "Any network request that may require DNS resolution, including "
          "navigations, connecting to a proxy server, detecting proxy "
          "settings, getting proxy config, certificate checking, and more."
        data:
          "Domain name that needs resolution."
        destination: OTHER
        destination_other:
          "The connection is made to a DNS server based on user's network "
          "settings."
        last_reviewed: "2026-02-17"
        internal {
          contacts {
            owners: "//net/dns/OWNERS"
          }
          contacts {
            owners: "//net/OWNERS"
          }
        }
        user_data {
          type: SENSITIVE_URL
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled. Without DNS Transactions Chrome "
          "cannot resolve host names."
        policy_exception_justification:
          "Essential for Chrome's navigation."
      })");

}  // namespace net
