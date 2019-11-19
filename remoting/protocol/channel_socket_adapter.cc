// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_socket_adapter.h"

#include <limits>

#include "base/callback.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

TransportChannelSocketAdapter::TransportChannelSocketAdapter(
    cricket::IceTransportInternal* ice_transport)
    : channel_(ice_transport), closed_error_code_(net::OK) {
  DCHECK(channel_);

  channel_->SignalReadPacket.connect(
      this, &TransportChannelSocketAdapter::OnNewPacket);
  channel_->SignalWritableState.connect(
      this, &TransportChannelSocketAdapter::OnWritableState);
  channel_->SignalDestroyed.connect(
      this, &TransportChannelSocketAdapter::OnChannelDestroyed);
}

TransportChannelSocketAdapter::~TransportChannelSocketAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!destruction_callback_.is_null())
    destruction_callback_.Run();
}

void TransportChannelSocketAdapter::SetOnDestroyedCallback(
    const base::Closure& callback) {
  destruction_callback_ = callback;
}

int TransportChannelSocketAdapter::Recv(
    const scoped_refptr<net::IOBuffer>& buf,
    int buffer_size,
    const net::CompletionRepeatingCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DCHECK(thread_checker_.CalledOnValidThread());
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
      if (result == net::ERR_IO_PENDING)
        result = net::OK;
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
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!channel_)  // Already closed.
    return;

  DCHECK(error_code != net::OK);
  closed_error_code_ = error_code;
  channel_->SignalReadPacket.disconnect(this);
  channel_->SignalDestroyed.disconnect(this);
  channel_ = NULL;

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
    rtc::PacketTransportInterface* transport,
    const char* data,
    size_t data_size,
    const int64_t& packet_time,
    int flags) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(transport, channel_);
  if (!read_callback_.is_null()) {
    DCHECK(read_buffer_.get());
    CHECK_LT(data_size, static_cast<size_t>(std::numeric_limits<int>::max()));

    if (read_buffer_size_ < static_cast<int>(data_size)) {
      LOG(WARNING) << "Data buffer is smaller than the received packet. "
                   << "Dropping the data that doesn't fit.";
      data_size = read_buffer_size_;
    }

    memcpy(read_buffer_->data(), data, data_size);

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
    rtc::PacketTransportInterface* transport) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Try to send the packet if there is a pending write.
  if (!write_callback_.is_null()) {
    rtc::PacketOptions options;
    int result = channel_->SendPacket(write_buffer_->data(),
                                      write_buffer_size_,
                                      options);
    if (result < 0)
      result = net::MapSystemError(channel_->GetError());

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
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(channel, channel_);
  Close(net::ERR_CONNECTION_ABORTED);
}

}  // namespace protocol
}  // namespace remoting
