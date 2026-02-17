// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TCP_ATTEMPT_H_
#define NET_DNS_DNS_TCP_ATTEMPT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// An implementation of DnsAttempt over a TCP transport.
class DnsTCPAttempt : public DnsAttempt {
 public:
  DnsTCPAttempt(size_t server_index,
                std::unique_ptr<StreamSocket> socket,
                std::unique_ptr<DnsQuery> query);
  ~DnsTCPAttempt() override;

  DnsTCPAttempt(const DnsTCPAttempt&) = delete;
  DnsTCPAttempt& operator=(const DnsTCPAttempt&) = delete;

  // DnsAttempt:
  int Start(CompletionOnceCallback callback) override;
  const DnsQuery* GetQuery() const override;

  const DnsResponse* GetResponse() const override;
  base::Value GetRawResponseBufferForLog() const override;
  const NetLogWithSource& GetSocketNetLog() const override;
  bool IsPending() const override;

 private:
  enum State {
    STATE_CONNECT_COMPLETE,
    STATE_SEND_LENGTH,
    STATE_SEND_QUERY,
    STATE_READ_LENGTH,
    STATE_READ_LENGTH_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result);

  int DoConnectComplete(int rv);
  int DoSendLength(int rv);
  int DoSendQuery(int rv);
  int DoReadLength(int rv);
  int DoReadLengthComplete(int rv);
  int DoReadResponse(int rv);

  int DoReadResponseComplete(int rv);

  void OnIOComplete(int rv);
  int ReadIntoBuffer();

  State next_state_ = STATE_NONE;
  base::TimeTicks start_time_;

  std::unique_ptr<StreamSocket> socket_;
  std::unique_ptr<DnsQuery> query_;
  scoped_refptr<IOBufferWithSize> length_buffer_;
  scoped_refptr<DrainableIOBuffer> buffer_;

  uint16_t response_length_ = 0;
  std::unique_ptr<DnsResponse> response_;

  CompletionOnceCallback callback_;
};

}  // namespace net

#endif  // NET_DNS_DNS_TCP_ATTEMPT_H_
