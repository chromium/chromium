// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_ATTEMPT_H_
#define NET_DNS_DNS_ATTEMPT_H_

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

// A single asynchronous DNS exchange, which consists of sending out a DNS
// query, waiting for a response, and returning the response that it matches.
class DnsAttempt {
 public:
  static Error FailureRcodeToNetError(int rcode);

  static base::DictValue NetLogStartParams(const std::string& hostname,
                                           uint16_t qtype);

  static const net::NetworkTrafficAnnotationTag kTrafficAnnotation;

  explicit DnsAttempt(size_t server_index);

  DnsAttempt(const DnsAttempt&) = delete;
  DnsAttempt& operator=(const DnsAttempt&) = delete;

  virtual ~DnsAttempt() = default;
  // Starts the attempt. Returns ERR_IO_PENDING if cannot complete synchronously
  // and calls |callback| upon completion.
  virtual int Start(CompletionOnceCallback callback) = 0;

  // Returns the query of this attempt.
  virtual const DnsQuery* GetQuery() const = 0;

  // Returns the response or NULL if has not received a matching response from
  // the server.
  virtual const DnsResponse* GetResponse() const = 0;

  virtual base::Value GetRawResponseBufferForLog() const = 0;

  // Returns the net log bound to the source of the socket.
  virtual const NetLogWithSource& GetSocketNetLog() const = 0;

  // Returns the index of the destination server within DnsConfig::nameservers
  // (or DnsConfig::dns_over_https_servers for secure transactions).
  size_t server_index() const;

  // Returns a Value representing the received response, along with a reference
  // to the NetLog source source of the UDP socket used.  The request must have
  // completed before this is called.
  base::DictValue NetLogResponseParams(NetLogCaptureMode capture_mode) const;

  // True if current attempt is pending (waiting for server response).
  virtual bool IsPending() const = 0;

 private:
  const size_t server_index_;
};

}  // namespace net

#endif  // NET_DNS_DNS_ATTEMPT_H_
