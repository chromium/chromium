// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_tcp_attempt.h"

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
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

DnsTCPAttempt::DnsTCPAttempt::DnsTCPAttempt(
    size_t server_index,
    std::unique_ptr<StreamSocket> socket,
    std::unique_ptr<DnsQuery> query)
    : DnsAttempt(server_index),
      socket_(std::move(socket)),
      query_(std::move(query)),
      length_buffer_(base::MakeRefCounted<IOBufferWithSize>(sizeof(uint16_t))) {
}

DnsTCPAttempt::~DnsTCPAttempt() = default;

int DnsTCPAttempt::Start(CompletionOnceCallback callback) {
  DCHECK_EQ(STATE_NONE, next_state_);
  callback_ = std::move(callback);
  start_time_ = base::TimeTicks::Now();
  next_state_ = STATE_CONNECT_COMPLETE;
  int rv = socket_->Connect(
      base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    return rv;
  }
  return DoLoop(rv);
}

const DnsQuery* DnsTCPAttempt::GetQuery() const {
  return query_.get();
}

const DnsResponse* DnsTCPAttempt::GetResponse() const {
  const DnsResponse* resp = response_.get();
  return (resp != nullptr && resp->IsValid()) ? resp : nullptr;
}

base::Value DnsTCPAttempt::GetRawResponseBufferForLog() const {
  if (!response_) {
    return base::Value();
  }

  return NetLogBinaryValue(response_->io_buffer()->data(),
                           response_->io_buffer_size());
}

const NetLogWithSource& DnsTCPAttempt::GetSocketNetLog() const {
  return socket_->NetLog();
}

bool DnsTCPAttempt::IsPending() const {
  return next_state_ != STATE_NONE;
}

int DnsTCPAttempt::DoLoop(int result) {
  CHECK_NE(STATE_NONE, next_state_);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_SEND_LENGTH:
        rv = DoSendLength(rv);
        break;
      case STATE_SEND_QUERY:
        rv = DoSendQuery(rv);
        break;
      case STATE_READ_LENGTH:
        rv = DoReadLength(rv);
        break;
      case STATE_READ_LENGTH_COMPLETE:
        rv = DoReadLengthComplete(rv);
        break;
      case STATE_READ_RESPONSE:
        rv = DoReadResponse(rv);
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

int DnsTCPAttempt::DoConnectComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }

  uint16_t query_size = static_cast<uint16_t>(query_->io_buffer()->size());
  if (static_cast<int>(query_size) != query_->io_buffer()->size()) {
    return ERR_FAILED;
  }
  length_buffer_->span().copy_from(base::U16ToBigEndian(query_size));
  buffer_ = base::MakeRefCounted<DrainableIOBuffer>(length_buffer_,
                                                    length_buffer_->size());
  next_state_ = STATE_SEND_LENGTH;
  return OK;
}

int DnsTCPAttempt::DoSendLength(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }

  buffer_->DidConsume(rv);
  if (buffer_->BytesRemaining() > 0) {
    next_state_ = STATE_SEND_LENGTH;
    return socket_->Write(
        buffer_.get(), buffer_->BytesRemaining(),
        base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)),
        kTrafficAnnotation);
  }
  buffer_ = base::MakeRefCounted<DrainableIOBuffer>(
      query_->io_buffer(), query_->io_buffer()->size());
  next_state_ = STATE_SEND_QUERY;
  return OK;
}

int DnsTCPAttempt::DoSendQuery(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }

  buffer_->DidConsume(rv);
  if (buffer_->BytesRemaining() > 0) {
    next_state_ = STATE_SEND_QUERY;
    return socket_->Write(
        buffer_.get(), buffer_->BytesRemaining(),
        base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)),
        kTrafficAnnotation);
  }
  buffer_ = base::MakeRefCounted<DrainableIOBuffer>(length_buffer_,
                                                    length_buffer_->size());
  next_state_ = STATE_READ_LENGTH;
  return OK;
}

int DnsTCPAttempt::DoReadLength(int rv) {
  DCHECK_EQ(OK, rv);

  next_state_ = STATE_READ_LENGTH_COMPLETE;
  return ReadIntoBuffer();
}

int DnsTCPAttempt::DoReadLengthComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }
  if (rv == 0) {
    return ERR_CONNECTION_CLOSED;
  }

  buffer_->DidConsume(rv);
  if (buffer_->BytesRemaining() > 0) {
    next_state_ = STATE_READ_LENGTH;
    return OK;
  }

  response_length_ = base::U16FromBigEndian(length_buffer_->span().first<2u>());
  // Check if advertised response is too short. (Optimization only.)
  if (response_length_ < query_->io_buffer()->size()) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }
  response_ = std::make_unique<DnsResponse>(response_length_);
  buffer_ = base::MakeRefCounted<DrainableIOBuffer>(response_->io_buffer(),
                                                    response_length_);
  next_state_ = STATE_READ_RESPONSE;
  return OK;
}

int DnsTCPAttempt::DoReadResponse(int rv) {
  DCHECK_EQ(OK, rv);

  next_state_ = STATE_READ_RESPONSE_COMPLETE;
  return ReadIntoBuffer();
}

int DnsTCPAttempt::DoReadResponseComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0) {
    return rv;
  }
  if (rv == 0) {
    return ERR_CONNECTION_CLOSED;
  }

  buffer_->DidConsume(rv);
  if (buffer_->BytesRemaining() > 0) {
    next_state_ = STATE_READ_RESPONSE;
    return OK;
  }
  DCHECK_GT(buffer_->BytesConsumed(), 0);
  if (!response_->InitParse(buffer_->BytesConsumed(), *query_)) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }
  if (response_->flags() & dns_protocol::kFlagTC) {
    return ERR_UNEXPECTED;
  }
  if (response_->rcode() != dns_protocol::kRcodeNOERROR) {
    // TODO(szym): Frankly, none of these are expected.
    return FailureRcodeToNetError(response_->rcode());
  }

  return OK;
}

void DnsTCPAttempt::OnIOComplete(int rv) {
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING) {
    std::move(callback_).Run(rv);
  }
}

int DnsTCPAttempt::ReadIntoBuffer() {
  return socket_->Read(
      buffer_.get(), buffer_->BytesRemaining(),
      base::BindOnce(&DnsTCPAttempt::OnIOComplete, base::Unretained(this)));
}

}  // namespace net
