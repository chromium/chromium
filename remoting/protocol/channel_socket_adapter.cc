// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_socket_adapter.h"

#include <limits>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace remoting::protocol {

TransportChannelSocketAdapter::TransportChannelSocketAdapter(
    cricket::IceTransportInternal* ice_transport)
    : channel_(ice_transport) {
  DCHECK(channel_);

  channel_->RegisterReceivedPacketCallback(
      this, [&](rtc::PacketTransportInternal* transport,
                const rtc::ReceivedPacket& packet) {
        OnNewPacket(transport, packet);
      });
  channel_->SignalWritableState.connect(
      this, &TransportChannelSocketAdapter::OnWritableState);
  channel_->SignalDestroyed.connect(
      this, &TransportChannelSocketAdapter::OnChannelDestroyed);
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

int TransportChannelSocketAdapter::Recv(
    const scoped_refptr<net::IOBuffer>& buf,
    int buffer_size,
    const net::CompletionRepeatingCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buf);
  DCHECK(!callback.is_null());
  CHECK(read_callback_.is_null());

  if (!channel_) {
    DCHECK(closed_error_code_ != net::OK);
    return closed_error_code_;
  }

  read_callback_ = callback;
  read_buffer_ = buf;
  read_buffer_size_ = buffer_size;

  return net::ERR_IO_PENDING;
}

int TransportChannelSocketAdapter::Send(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_size,
    const net::CompletionRepeatingCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(!callback.is_null());
  CHECK(write_callback_.is_null());

  if (!channel_) {
    DCHECK(closed_error_code_ != net::OK);
    return closed_error_code_;
  }

  int result;
  rtc::PacketOptions options;
  if (channel_->writable()) {
    result = channel_->SendPacket(buffer->data(), buffer_size, options);
    if (result < 0) {
      result = net::MapSystemError(channel_->GetError());

      // If the underlying socket returns IO pending where it shouldn't we
      // pretend the packet is dropped and return as succeeded because no
      // writeable callback will happen.
      if (result == net::ERR_IO_PENDING) {
        result = net::OK;
      }
    }
  } else {
    // Channel is not writable yet.
    result = net::ERR_IO_PENDING;
    write_callback_ = callback;
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
  }

  return result;
}

void TransportChannelSocketAdapter::Close(int error_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!channel_) {  // Already closed.
    return;
  }

  DCHECK(error_code != net::OK);
  closed_error_code_ = error_code;
  channel_->DeregisterReceivedPacketCallback(this);
  channel_->SignalDestroyed.disconnect(this);
  channel_ = nullptr;

  if (!read_callback_.is_null()) {
    net::CompletionRepeatingCallback callback = read_callback_;
    read_callback_.Reset();
    read_buffer_.reset();
    callback.Run(error_code);
  }

  if (!write_callback_.is_null()) {
    net::CompletionRepeatingCallback callback = write_callback_;
    write_callback_.Reset();
    write_buffer_.reset();
    callback.Run(error_code);
  }
}

void TransportChannelSocketAdapter::OnNewPacket(
    rtc::PacketTransportInternal* transport,
    const rtc::ReceivedPacket& packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(transport, channel_);
  if (!read_callback_.is_null()) {
    DCHECK(read_buffer_.get());
    CHECK_LT(packet.payload().size(),
             static_cast<size_t>(std::numeric_limits<int>::max()));
    size_t data_size = packet.payload().size();
    if (read_buffer_size_ < static_cast<int>(data_size)) {
      LOG(WARNING) << "Data buffer is smaller than the received packet. "
                   << "Dropping the data that doesn't fit.";
      data_size = read_buffer_size_;
    }

    memcpy(read_buffer_->data(), packet.payload().data(), data_size);

    net::CompletionRepeatingCallback callback = read_callback_;
    read_callback_.Reset();
    read_buffer_.reset();
    callback.Run(data_size);
  } else {
    LOG(WARNING)
        << "Data was received without a callback. Dropping the packet.";
  }
}

void TransportChannelSocketAdapter::OnWritableState(
    rtc::PacketTransportInternal* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Try to send the packet if there is a pending write.
  if (!write_callback_.is_null()) {
    rtc::PacketOptions options;
    int result = channel_->SendPacket(write_buffer_->data(), write_buffer_size_,
                                      options);
    if (result < 0) {
      result = net::MapSystemError(channel_->GetError());
    }

    if (result != net::ERR_IO_PENDING) {
      net::CompletionRepeatingCallback callback = write_callback_;
      write_callback_.Reset();
      write_buffer_.reset();
      callback.Run(result);
    }
  }
}

void TransportChannelSocketAdapter::OnChannelDestroyed(
    cricket::IceTransportInternal* channel) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(channel, channel_);
  Close(net::ERR_CONNECTION_ABORTED);
}

}  // namespace remoting::protocol
