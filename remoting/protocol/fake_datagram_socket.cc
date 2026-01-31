// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_datagram_socket.h"

#include <cstddef>
#include <utility>

#include "base/byte_size.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

FakeDatagramSocket::FakeDatagramSocket()
    : input_pos_(0),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

FakeDatagramSocket::~FakeDatagramSocket() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
}

void FakeDatagramSocket::AppendInputPacket(const std::string& data) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  input_packets_.push_back(data);

  // Complete pending read if any.
  if (!read_callback_.is_null()) {
    DCHECK_EQ(input_pos_, static_cast<int>(input_packets_.size()) - 1);
    base::ByteSize result = CopyReadData(read_buffer_.get(), read_buffer_size_);
    read_buffer_ = nullptr;

    std::move(read_callback_).Run(result);
  }
}

void FakeDatagramSocket::PairWith(FakeDatagramSocket* peer_socket) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  peer_socket_ = peer_socket->GetWeakPtr();
  peer_socket->peer_socket_ = GetWeakPtr();
}

base::WeakPtr<FakeDatagramSocket> FakeDatagramSocket::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::expected<base::ByteSize, net::Error> FakeDatagramSocket::Recv(
    const scoped_refptr<net::IOBuffer>& buf,
    base::ByteSize buf_len,
    P2PDatagramSocket::Callback callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  if (input_pos_ < static_cast<int>(input_packets_.size())) {
    return CopyReadData(buf, buf_len);
  } else {
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return base::unexpected(net::ERR_IO_PENDING);
  }
}

base::expected<base::ByteSize, net::Error> FakeDatagramSocket::Send(
    const scoped_refptr<net::IOBuffer>& buf,
    base::ByteSize buf_len,
    P2PDatagramSocket::Callback callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  EXPECT_FALSE(send_pending_);

  if (async_send_) {
    send_pending_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDatagramSocket::DoAsyncSend,
                       weak_factory_.GetWeakPtr(), buf, buf_len, callback));
    return base::unexpected(net::ERR_IO_PENDING);
  } else {
    return DoSend(buf, buf_len);
  }
}

void FakeDatagramSocket::DoAsyncSend(const scoped_refptr<net::IOBuffer>& buf,
                                     base::ByteSize buf_len,
                                     P2PDatagramSocket::Callback callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

  EXPECT_TRUE(send_pending_);
  send_pending_ = false;
  callback.Run(DoSend(buf, buf_len));
}

base::expected<base::ByteSize, net::Error> FakeDatagramSocket::DoSend(
    const scoped_refptr<net::IOBuffer>& buf,
    base::ByteSize buf_len) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

  if (next_send_error_ != net::OK) {
    net::Error r = next_send_error_;
    next_send_error_ = net::OK;
    return base::unexpected(r);
  }

  auto packet = buf->first(buf_len.InBytes());
  written_packets_.emplace_back(base::as_string_view(packet));

  if (peer_socket_.get()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDatagramSocket::AppendInputPacket, peer_socket_,
                       std::string(base::as_string_view(packet))));
  }

  return buf_len;
}

base::ByteSize FakeDatagramSocket::CopyReadData(
    const scoped_refptr<net::IOBuffer>& buf,
    base::ByteSize buf_len) {
  size_t read_size =
      std::min<size_t>(buf_len.InBytes(), input_packets_[input_pos_].size());
  buf->span().copy_prefix_from(
      base::as_byte_span(input_packets_[input_pos_]).first(read_size));
  ++input_pos_;
  return base::ByteSize(read_size);
}

FakeDatagramChannelFactory::FakeDatagramChannelFactory()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      asynchronous_create_(false),
      fail_create_(false) {}

FakeDatagramChannelFactory::~FakeDatagramChannelFactory() {
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    EXPECT_FALSE(it->second);
  }
}

void FakeDatagramChannelFactory::PairWith(
    FakeDatagramChannelFactory* peer_factory) {
  peer_factory_ = peer_factory->weak_factory_.GetWeakPtr();
  peer_factory_->peer_factory_ = weak_factory_.GetWeakPtr();
}

FakeDatagramSocket* FakeDatagramChannelFactory::GetFakeChannel(
    const std::string& name) {
  return channels_[name].get();
}

void FakeDatagramChannelFactory::CreateChannel(
    const std::string& name,
    ChannelCreatedCallback callback) {
  EXPECT_FALSE(channels_[name]);

  std::unique_ptr<FakeDatagramSocket> channel(new FakeDatagramSocket());
  channels_[name] = channel->GetWeakPtr();

  if (peer_factory_) {
    FakeDatagramSocket* peer_socket = peer_factory_->GetFakeChannel(name);
    if (peer_socket) {
      channel->PairWith(peer_socket);
    }
  }

  if (fail_create_) {
    channel.reset();
  }

  if (asynchronous_create_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDatagramChannelFactory::NotifyChannelCreated,
                       weak_factory_.GetWeakPtr(), std::move(channel), name,
                       std::move(callback)));
  } else {
    NotifyChannelCreated(std::move(channel), name, std::move(callback));
  }
}

void FakeDatagramChannelFactory::NotifyChannelCreated(
    std::unique_ptr<FakeDatagramSocket> owned_socket,
    const std::string& name,
    ChannelCreatedCallback callback) {
  if (channels_.contains(name)) {
    std::move(callback).Run(std::move(owned_socket));
  }
}

void FakeDatagramChannelFactory::CancelChannelCreation(
    const std::string& name) {
  channels_.erase(name);
}

}  // namespace remoting::protocol
