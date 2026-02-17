// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_udp_attempt.h"

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
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

DnsUDPAttempt::DnsUDPAttempt(size_t server_index,
                             std::unique_ptr<DatagramClientSocket> socket,
                             const IPEndPoint& server,
                             std::unique_ptr<DnsQuery> query,
                             DnsUdpTracker* udp_tracker)
    : DnsAttempt(server_index),
      socket_(std::move(socket)),
      server_(server),
      query_(std::move(query)),
      udp_tracker_(udp_tracker) {}

DnsUDPAttempt::~DnsUDPAttempt() = default;

int DnsUDPAttempt::Start(CompletionOnceCallback callback) {
  DCHECK_EQ(STATE_NONE, next_state_);
  callback_ = std::move(callback);
  start_time_ = base::TimeTicks::Now();
  next_state_ = STATE_CONNECT_COMPLETE;

  int rv = socket_->ConnectAsync(
      server_,
      base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    return rv;
  }
  return DoLoop(rv);
}

const DnsQuery* DnsUDPAttempt::GetQuery() const {
  return query_.get();
}

const DnsResponse* DnsUDPAttempt::GetResponse() const {
  const DnsResponse* resp = response_.get();
  return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
}

base::Value DnsUDPAttempt::GetRawResponseBufferForLog() const {
  if (!response_) {
    return base::Value();
  }
  return NetLogBinaryValue(response_->io_buffer()->data(), read_size_);
}

const NetLogWithSource& DnsUDPAttempt::GetSocketNetLog() const {
  return socket_->NetLog();
}

bool DnsUDPAttempt::IsPending() const {
  return next_state_ != STATE_NONE;
}

int DnsUDPAttempt::DoLoop(int result) {
  CHECK_NE(STATE_NONE, next_state_);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_SEND_QUERY:
        rv = DoSendQuery(rv);
        break;
      case STATE_SEND_QUERY_COMPLETE:
        rv = DoSendQueryComplete(rv);
        break;
      case STATE_READ_RESPONSE:
        rv = DoReadResponse();
        break;
      case STATE_READ_RESPONSE_COMPLETE:
        rv = DoReadResponseComplete(rv);
        break;
      default:
        NOTREACHED();
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  if (rv != ERR_IO_PENDING) {
    DCHECK_EQ(STATE_NONE, next_state_);
  }

  return rv;
}

int DnsUDPAttempt::DoConnectComplete(int rv) {
  if (rv != OK) {
    DVLOG(1) << "Failed to connect socket: " << rv;
    udp_tracker_->RecordConnectionError(rv);
    return ERR_CONNECTION_REFUSED;
  }
  next_state_ = STATE_SEND_QUERY;
  IPEndPoint local_address;
  if (socket_->GetLocalAddress(&local_address) == OK) {
    udp_tracker_->RecordQuery(local_address.port(), query_->id());
  }
  return OK;
}

int DnsUDPAttempt::DoSendQuery(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }
  next_state_ = STATE_SEND_QUERY_COMPLETE;
  return socket_->Write(
      query_->io_buffer(), query_->io_buffer()->size(),
      base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)),
      kTrafficAnnotation);
}

int DnsUDPAttempt::DoSendQueryComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }

  // Writing to UDP should not result in a partial datagram.
  if (rv != query_->io_buffer()->size()) {
    return ERR_MSG_TOO_BIG;
  }

  next_state_ = STATE_READ_RESPONSE;
  return OK;
}

int DnsUDPAttempt::DoReadResponse() {
  next_state_ = STATE_READ_RESPONSE_COMPLETE;
  response_ = std::make_unique<DnsResponse>();
  return socket_->Read(
      response_->io_buffer(), response_->io_buffer_size(),
      base::BindOnce(&DnsUDPAttempt::OnIOComplete, base::Unretained(this)));
}

int DnsUDPAttempt::DoReadResponseComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }
  read_size_ = rv;

  bool parse_result = response_->InitParse(rv, *query_);
  if (response_->id()) {
    udp_tracker_->RecordResponseId(query_->id(), response_->id().value());
  }

  if (!parse_result) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }
  if (response_->flags() & dns_protocol::kFlagTC) {
    return ERR_DNS_SERVER_REQUIRES_TCP;
  }
  if (response_->rcode() != dns_protocol::kRcodeNOERROR) {
    return FailureRcodeToNetError(response_->rcode());
  }

  return OK;
}

void DnsUDPAttempt::OnIOComplete(int rv) {
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING) {
    std::move(callback_).Run(rv);
  }
}

}  // namespace net
