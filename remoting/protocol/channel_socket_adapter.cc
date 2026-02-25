// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_socket_adapter.h"

#include <limits>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace remoting::protocol {

TransportChannelSocketAdapter::TransportChannelSocketAdapter(
    webrtc::IceTransportInternal* ice_transport)
    : channel_(ice_transport) {
  DCHECK(channel_);

  channel_->RegisterReceivedPacketCallback(
      this, [&](webrtc::PacketTransportInternal* transport,
                const webrtc::ReceivedIpPacket& packet) {
        OnNewPacket(transport, packet);
      });
  channel_->SubscribeWritableState(
      this, [this](webrtc::PacketTransportInternal* transport) {
        OnWritableState(transport);
      });
}

TransportChannelSocketAdapter::~TransportChannelSocketAdapter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
  if (channel_) {
    // Channel may still exist in unit tests. We must deregister packet callback
    // in order to prevent callbacks after destruction.
    channel_->DeregisterReceivedPacketCallback(this);
  }
}

void TransportChannelSocketAdapter::SetOnDestroyedCallback(
    base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

base::expected<base::ByteSize, net::Error> TransportChannelSocketAdapter::Recv(
    const scoped_refptr<net::IOBuffer>& buf,
    base::ByteSize buf_len,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buf);
  DCHECK(!callback.is_null());
  CHECK(read_callback_.is_null());

  if (!channel_) {
    DCHECK_NE(closed_error_code_, net::OK);
    return base::unexpected(closed_error_code_);
  }

  read_callback_ = callback;
  read_buffer_ = buf;
  read_buffer_size_ = buf_len;

  return base::unexpected(net::ERR_IO_PENDING);
}

base::expected<base::ByteSize, net::Error> TransportChannelSocketAdapter::Send(
    const scoped_refptr<net::IOBuffer>& buf,
    base::ByteSize buf_len,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buf);
  DCHECK(!callback.is_null());
  CHECK(write_callback_.is_null());

  if (!channel_) {
    DCHECK_NE(closed_error_code_, net::OK);
    return base::unexpected(closed_error_code_);
  }

  base::expected<base::ByteSize, net::Error> result;
  webrtc::AsyncSocketPacketOptions options;
  if (channel_->writable()) {
    int channel_result =
        channel_->SendPacket(buf->data(), buf_len.InBytes(), options);
    if (channel_result < 0) {
      result = base::unexpected(net::MapSystemError(channel_->GetError()));

      // If the underlying socket returns IO pending where it shouldn't we
      // pretend the packet is dropped and return as succeeded because no
      // writeable callback will happen.
      if (channel_result == net::ERR_IO_PENDING) {
        result = base::ByteSize(0);
      }
    } else {
      result = base::ByteSize(base::checked_cast<uint64_t>(channel_result));
    }
  } else {
    // Channel is not writable yet.
    result = base::unexpected(net::ERR_IO_PENDING);
    write_callback_ = callback;
    write_buffer_ = buf;
    write_buffer_size_ = buf_len;
  }

  return result;
}

void TransportChannelSocketAdapter::Close(net::Error error_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!channel_) {  // Already closed.
    return;
  }

  DCHECK_NE(error_code, net::OK);
  closed_error_code_ = error_code;
  channel_->DeregisterReceivedPacketCallback(this);
  channel_ = nullptr;

  if (!read_callback_.is_null()) {
    P2PDatagramSocket::Callback callback = read_callback_;
    read_callback_.Reset();
    read_buffer_.reset();
    callback.Run(base::unexpected(error_code));
  }

  if (!write_callback_.is_null()) {
    P2PDatagramSocket::Callback callback = write_callback_;
    write_callback_.Reset();
    write_buffer_.reset();
    callback.Run(base::unexpected(error_code));
  }
}

void TransportChannelSocketAdapter::OnNewPacket(
    webrtc::PacketTransportInternal* transport,
    const webrtc::ReceivedIpPacket& packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(transport, channel_);
  if (!read_callback_.is_null()) {
    DCHECK(read_buffer_.get());
    CHECK_LT(packet.payload().size(),
             static_cast<size_t>(std::numeric_limits<int>::max()));
    base::ByteSize data_size = base::ByteSize(packet.payload().size());
    if (read_buffer_size_ < base::ByteSize(data_size)) {
      LOG(WARNING) << "Data buffer is smaller than the received packet. "
                   << "Dropping the data that doesn't fit.";
      data_size = read_buffer_size_;
    }

    read_buffer_->span().copy_prefix_from(
        packet.payload().subspan(0, data_size.InBytes()));

    P2PDatagramSocket::Callback callback = read_callback_;
    read_callback_.Reset();
    read_buffer_.reset();
    callback.Run(data_size);
  } else {
    LOG(WARNING)
        << "Data was received without a callback. Dropping the packet.";
  }
}

void TransportChannelSocketAdapter::OnWritableState(
    webrtc::PacketTransportInternal* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Try to send the packet if there is a pending write.
  if (!write_callback_.is_null()) {
    base::expected<base::ByteSize, net::Error> result;
    webrtc::AsyncSocketPacketOptions options;
    int channel_result = channel_->SendPacket(
        write_buffer_->data(), write_buffer_size_.InBytes(), options);
    if (channel_result < 0) {
      result = base::unexpected(net::MapSystemError(channel_->GetError()));
    } else {
      result = base::ByteSize(base::checked_cast<uint64_t>(channel_result));
    }

    if (result.has_value() || result.error() != net::ERR_IO_PENDING) {
      P2PDatagramSocket::Callback callback = write_callback_;
      write_callback_.Reset();
      write_buffer_.reset();
      callback.Run(result);
    }
  }
}

}  // namespace remoting::protocol
