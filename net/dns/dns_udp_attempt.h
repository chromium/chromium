// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_UDP_ATTEMPT_H_
#define NET_DNS_DNS_UDP_ATTEMPT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_udp_tracker.h"
#include "net/dns/public/dns_protocol.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// An implementation of DnsAttempt over a UDP transport.
class DnsUDPAttempt : public DnsAttempt {
 public:
  DnsUDPAttempt(size_t server_index,
                std::unique_ptr<DatagramClientSocket> socket,
                const IPEndPoint& server,
                std::unique_ptr<DnsQuery> query,
                DnsUdpTracker* udp_tracker);
  ~DnsUDPAttempt() override;

  DnsUDPAttempt(const DnsUDPAttempt&) = delete;
  DnsUDPAttempt& operator=(const DnsUDPAttempt&) = delete;

  // DnsAttempt methods.
  int Start(CompletionOnceCallback callback) override;
  const DnsQuery* GetQuery() const override;
  const DnsResponse* GetResponse() const override;
  base::Value GetRawResponseBufferForLog() const override;
  const NetLogWithSource& GetSocketNetLog() const override;
  bool IsPending() const override;

 private:
  enum State {
    STATE_CONNECT_COMPLETE,
    STATE_SEND_QUERY,
    STATE_SEND_QUERY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result);

  int DoConnectComplete(int rv);
  int DoSendQuery(int rv);
  int DoSendQueryComplete(int rv);
  int DoReadResponse();
  int DoReadResponseComplete(int rv);
  void OnIOComplete(int rv);

  State next_state_ = STATE_NONE;
  base::TimeTicks start_time_;

  std::unique_ptr<DatagramClientSocket> socket_;
  IPEndPoint server_;
  std::unique_ptr<DnsQuery> query_;

  // Should be owned by the DnsSession, to which the transaction should own a
  // reference.
  const raw_ptr<DnsUdpTracker> udp_tracker_;

  std::unique_ptr<DnsResponse> response_;
  int read_size_ = 0;

  CompletionOnceCallback callback_;
};

}  // namespace net

#endif  // NET_DNS_DNS_UDP_ATTEMPT_H_
