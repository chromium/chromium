// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks5_client_socket.h"

#include <stdint.h>

#include <array>
#include <utility>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/sys_addrinfo.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

const unsigned int SOCKS5ClientSocket::kGreetReadHeaderSize = 2;
const unsigned int SOCKS5ClientSocket::kWriteHeaderSize = 10;
const unsigned int SOCKS5ClientSocket::kReadHeaderSize = 5;
const uint8_t SOCKS5ClientSocket::kSOCKS5Version = 0x05;
const uint8_t SOCKS5ClientSocket::kTunnelCommand = 0x01;
const uint8_t SOCKS5ClientSocket::kNullByte = 0x00;

static_assert(sizeof(struct in_addr) == 4, "incorrect system size of IPv4");
static_assert(sizeof(struct in6_addr) == 16, "incorrect system size of IPv6");

SOCKS5ClientSocket::SOCKS5ClientSocket(
    std::unique_ptr<StreamSocket> transport_socket,
    const HostPortPair& destination,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : io_callback_(base::BindRepeating(&SOCKS5ClientSocket::OnIOComplete,
                                       base::Unretained(this))),
      transport_socket_(std::move(transport_socket)),
      destination_(destination),
      net_log_(transport_socket_->NetLog()),
      traffic_annotation_(traffic_annotation) {}

SOCKS5ClientSocket::~SOCKS5ClientSocket() {
  Disconnect();
}

int SOCKS5ClientSocket::Connect(CompletionOnceCallback callback) {
  DCHECK(transport_socket_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(user_callback_.is_null());

  // If already connected, then just return OK.
  if (completed_handshake_)
    return OK;

  net_log_.BeginEvent(NetLogEventType::SOCKS5_CONNECT);

  next_state_ = STATE_GREET_WRITE;
  write_buf_.reset();
  read_buf_.reset();

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = std::move(callback);
  } else {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::SOCKS5_CONNECT, rv);
  }
  return rv;
}

void SOCKS5ClientSocket::Disconnect() {
  completed_handshake_ = false;
  transport_socket_->Disconnect();

  // Reset other states to make sure they aren't mistakenly used later.
  // These are the states initialized by Connect().
  next_state_ = STATE_NONE;
  user_callback_.Reset();
}

bool SOCKS5ClientSocket::IsConnected() const {
  return completed_handshake_ && transport_socket_->IsConnected();
}

bool SOCKS5ClientSocket::IsConnectedAndIdle() const {
  return completed_handshake_ && transport_socket_->IsConnectedAndIdle();
}

const NetLogWithSource& SOCKS5ClientSocket::NetLog() const {
  return net_log_;
}

bool SOCKS5ClientSocket::WasEverUsed() const {
  return was_ever_used_;
}

NextProto SOCKS5ClientSocket::GetNegotiatedProtocol() const {
  if (transport_socket_)
    return transport_socket_->GetNegotiatedProtocol();
  NOTREACHED();
}

bool SOCKS5ClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  if (transport_socket_)
    return transport_socket_->GetSSLInfo(ssl_info);
  NOTREACHED();
}

int64_t SOCKS5ClientSocket::GetTotalReceivedBytes() const {
  return transport_socket_->GetTotalReceivedBytes();
}

void SOCKS5ClientSocket::ApplySocketTag(const SocketTag& tag) {
  return transport_socket_->ApplySocketTag(tag);
}

// Read is called by the transport layer above to read. This can only be done
// if the SOCKS handshake is complete.
int SOCKS5ClientSocket::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(user_callback_.is_null());
  DCHECK(!callback.is_null());

  int rv = transport_socket_->Read(
      buf, buf_len,
      base::BindOnce(&SOCKS5ClientSocket::OnReadWriteComplete,
                     base::Unretained(this), std::move(callback)));
  if (rv > 0)
    was_ever_used_ = true;
  return rv;
}

// Write is called by the transport layer. This can only be done if the
// SOCKS handshake is complete.
int SOCKS5ClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(user_callback_.is_null());
  DCHECK(!callback.is_null());

  int rv = transport_socket_->Write(
      buf, buf_len,
      base::BindOnce(&SOCKS5ClientSocket::OnReadWriteComplete,
                     base::Unretained(this), std::move(callback)),
      traffic_annotation);
  if (rv > 0)
    was_ever_used_ = true;
  return rv;
}

int SOCKS5ClientSocket::SetReceiveBufferSize(int32_t size) {
  return transport_socket_->SetReceiveBufferSize(size);
}

int SOCKS5ClientSocket::SetSendBufferSize(int32_t size) {
  return transport_socket_->SetSendBufferSize(size);
}

void SOCKS5ClientSocket::DoCallback(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(!user_callback_.is_null());

  // Since Run() may result in Read being called,
  // clear user_callback_ up front.
  std::move(user_callback_).Run(result);
}

void SOCKS5ClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEvent(NetLogEventType::SOCKS5_CONNECT);
    DoCallback(rv);
  }
}

void SOCKS5ClientSocket::OnReadWriteComplete(CompletionOnceCallback callback,
                                             int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(!callback.is_null());

  if (result > 0)
    was_ever_used_ = true;
  std::move(callback).Run(result);
}

int SOCKS5ClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_GREET_WRITE:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::SOCKS5_GREET_WRITE);
        rv = DoGreetWrite();
        break;
      case STATE_GREET_WRITE_COMPLETE:
        rv = DoGreetWriteComplete(rv);
        net_log_.EndEventWithNetErrorCode(NetLogEventType::SOCKS5_GREET_WRITE,
                                          rv);
        break;
      case STATE_GREET_READ:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::SOCKS5_GREET_READ);
        rv = DoGreetRead();
        break;
      case STATE_GREET_READ_COMPLETE:
        rv = DoGreetReadComplete(rv);
        net_log_.EndEventWithNetErrorCode(NetLogEventType::SOCKS5_GREET_READ,
                                          rv);
        break;
      case STATE_HANDSHAKE_WRITE:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::SOCKS5_HANDSHAKE_WRITE);
        rv = DoHandshakeWrite();
        break;
      case STATE_HANDSHAKE_WRITE_COMPLETE:
        rv = DoHandshakeWriteComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::SOCKS5_HANDSHAKE_WRITE, rv);
        break;
      case STATE_HANDSHAKE_READ:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::SOCKS5_HANDSHAKE_READ);
        rv = DoHandshakeRead();
        break;
      case STATE_HANDSHAKE_READ_COMPLETE:
        rv = DoHandshakeReadComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::SOCKS5_HANDSHAKE_READ, rv);
        break;
      default:
        NOTREACHED() << "bad state";
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

static constexpr std::array<uint8_t, 3> kSOCKS5GreetWriteData{
    0x05, 0x01, 0x00};  // no authentication

int SOCKS5ClientSocket::DoGreetWrite() {
  // Since we only have 1 byte to send the hostname length in, if the
  // URL has a hostname longer than 255 characters we can't send it.
  if (0xFF < destination_.host().size()) {
    net_log_.AddEvent(NetLogEventType::SOCKS_HOSTNAME_TOO_BIG);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  if (!write_buf_) {
    auto greet_buffer =
        base::MakeRefCounted<WrappedIOBuffer>(kSOCKS5GreetWriteData);
    write_buf_ = base::MakeRefCounted<DrainableIOBuffer>(
        std::move(greet_buffer), greet_buffer->size());
  }

  next_state_ = STATE_GREET_WRITE_COMPLETE;
  return transport_socket_->Write(write_buf_.get(),
                                  write_buf_->BytesRemaining(), io_callback_,
                                  traffic_annotation_);
}

int SOCKS5ClientSocket::DoGreetWriteComplete(int result) {
  if (result < 0)
    return result;

  write_buf_->DidConsume(result);
  if (write_buf_->BytesRemaining() == 0) {
    write_buf_.reset();
    next_state_ = STATE_GREET_READ;
  } else {
    next_state_ = STATE_GREET_WRITE;
  }
  return OK;
}

int SOCKS5ClientSocket::DoGreetRead() {
  next_state_ = STATE_GREET_READ_COMPLETE;
  if (!read_buf_) {
    read_buf_ = base::MakeRefCounted<GrowableIOBuffer>();
    read_buf_->SetCapacity(kGreetReadHeaderSize);
  }
  return transport_socket_->Read(read_buf_.get(),
                                 read_buf_->RemainingCapacity(), io_callback_);
}

int SOCKS5ClientSocket::DoGreetReadComplete(int result) {
  if (result < 0)
    return result;

  if (result == 0) {
    net_log_.AddEvent(
        NetLogEventType::SOCKS_UNEXPECTEDLY_CLOSED_DURING_GREETING);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  read_buf_->set_offset(read_buf_->offset() + result);
  if (read_buf_->RemainingCapacity() > 0) {
    next_state_ = STATE_GREET_READ;
    return OK;
  }

  // Got the greet data.
  base::span<uint8_t> read_data = read_buf_->span_before_offset();

  if (read_data[0] != kSOCKS5Version) {
    net_log_.AddEventWithIntParams(NetLogEventType::SOCKS_UNEXPECTED_VERSION,
                                   "version", read_data[0]);
    return ERR_SOCKS_CONNECTION_FAILED;
  }
  if (read_data[1] != 0x00) {
    net_log_.AddEventWithIntParams(NetLogEventType::SOCKS_UNEXPECTED_AUTH,
                                   "method", read_data[1]);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  read_buf_.reset();
  next_state_ = STATE_HANDSHAKE_WRITE;
  return OK;
}

scoped_refptr<DrainableIOBuffer> SOCKS5ClientSocket::BuildHandshakeWriteBuffer()
    const {
  std::vector<uint8_t> handshake;
  handshake.reserve(7 + destination_.host().size());

  handshake.push_back(kSOCKS5Version);
  handshake.push_back(kTunnelCommand);  // Connect command
  handshake.push_back(kNullByte);       // Reserved null

  handshake.push_back(kEndPointDomain);  // The type of the address.

  // First add the size of the hostname, followed by the hostname. The length of
  // the hostname must fit within one byte.
  const auto& host = destination_.host();
  handshake.push_back(base::checked_cast<uint8_t>(host.size()));
  handshake.insert(handshake.end(), host.begin(), host.end());

  auto nw_port = base::U16ToBigEndian(destination_.port());
  handshake.insert(handshake.end(), nw_port.begin(), nw_port.end());

  auto base_buffer = base::MakeRefCounted<VectorIOBuffer>(std::move(handshake));
  return base::MakeRefCounted<DrainableIOBuffer>(std::move(base_buffer),
                                                 base_buffer->size());
}

// Writes the SOCKS handshake data to the underlying socket connection.
int SOCKS5ClientSocket::DoHandshakeWrite() {
  next_state_ = STATE_HANDSHAKE_WRITE_COMPLETE;

  if (!write_buf_) {
    write_buf_ = BuildHandshakeWriteBuffer();
  }

  return transport_socket_->Write(write_buf_.get(),
                                  write_buf_->BytesRemaining(), io_callback_,
                                  traffic_annotation_);
}

int SOCKS5ClientSocket::DoHandshakeWriteComplete(int result) {
  if (result < 0)
    return result;

  // We ignore the case when result is 0, since the underlying Write
  // may return spurious writes while waiting on the socket.

  write_buf_->DidConsume(result);
  if (write_buf_->BytesRemaining() == 0) {
    write_buf_.reset();
    next_state_ = STATE_HANDSHAKE_READ;
  } else {
    next_state_ = STATE_HANDSHAKE_WRITE;
  }

  return OK;
}

int SOCKS5ClientSocket::DoHandshakeRead() {
  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (!read_buf_) {
    read_buf_ = base::MakeRefCounted<GrowableIOBuffer>();
    read_buf_->SetCapacity(kReadHeaderSize);
  }

  return transport_socket_->Read(read_buf_.get(),
                                 read_buf_->RemainingCapacity(), io_callback_);
}

int SOCKS5ClientSocket::DoHandshakeReadComplete(int result) {
  if (result < 0)
    return result;

  // The underlying socket closed unexpectedly.
  if (result == 0) {
    net_log_.AddEvent(
        NetLogEventType::SOCKS_UNEXPECTEDLY_CLOSED_DURING_HANDSHAKE);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  read_buf_->set_offset(read_buf_->offset() + result);

  // When the first few bytes are read, check how many more are required
  // and accordingly increase them
  if (read_buf_->offset() == kReadHeaderSize) {
    base::span<uint8_t> read_data = read_buf_->span_before_offset();

    if (read_data[0] != kSOCKS5Version || read_data[2] != kNullByte) {
      net_log_.AddEventWithIntParams(NetLogEventType::SOCKS_UNEXPECTED_VERSION,
                                     "version", read_data[0]);
      return ERR_SOCKS_CONNECTION_FAILED;
    }
    if (read_data[1] != 0x00) {
      net_log_.AddEventWithIntParams(NetLogEventType::SOCKS_SERVER_ERROR,
                                     "error_code", read_data[1]);
      return ERR_SOCKS_CONNECTION_FAILED;
    }

    // We check the type of IP/Domain the server returns and accordingly
    // increase the size of the response. For domains, we need to read the
    // size of the domain, so the initial request size is upto the domain
    // size. Since for IPv4/IPv6 the size is fixed and hence no 'size' is
    // read, we substract 1 byte from the additional request size.
    SocksEndPointAddressType address_type =
        static_cast<SocksEndPointAddressType>(read_data[3]);
    int additional_bytes_expected = 0;
    if (address_type == kEndPointDomain) {
      additional_bytes_expected += read_data[4];
    } else if (address_type == kEndPointResolvedIPv4) {
      additional_bytes_expected += sizeof(struct in_addr) - 1;
    } else if (address_type == kEndPointResolvedIPv6) {
      additional_bytes_expected += sizeof(struct in6_addr) - 1;
    } else {
      net_log_.AddEventWithIntParams(
          NetLogEventType::SOCKS_UNKNOWN_ADDRESS_TYPE, "address_type",
          read_data[3]);
      return ERR_SOCKS_CONNECTION_FAILED;
    }

    additional_bytes_expected += 2;  // for the port.
    // Update capacity.
    read_buf_->SetCapacity(kReadHeaderSize + additional_bytes_expected);
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  // When the final bytes are read, setup handshake. We ignore the rest
  // of the response since they represent the SOCKSv5 endpoint and have
  // no use when doing a tunnel connection.
  if (read_buf_->RemainingCapacity() == 0) {
    completed_handshake_ = true;
    read_buf_.reset();
    next_state_ = STATE_NONE;
    return OK;
  }

  next_state_ = STATE_HANDSHAKE_READ;
  return OK;
}

int SOCKS5ClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return transport_socket_->GetPeerAddress(address);
}

int SOCKS5ClientSocket::GetLocalAddress(IPEndPoint* address) const {
  return transport_socket_->GetLocalAddress(address);
}

}  // namespace net
