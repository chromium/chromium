// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_datagram_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

FakeDatagramSocket::FakeDatagramSocket()
    : input_pos_(0), task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeDatagramSocket::~FakeDatagramSocket() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
}

void FakeDatagramSocket::AppendInputPacket(const std::string& data) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  input_packets_.push_back(data);

  // Complete pending read if any.
  if (!read_callback_.is_null()) {
    DCHECK_EQ(input_pos_, static_cast<int>(input_packets_.size()) - 1);
    int result = CopyReadData(read_buffer_.get(), read_buffer_size_);
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

int FakeDatagramSocket::Recv(const scoped_refptr<net::IOBuffer>& buf,
                             int buf_len,
                             const net::CompletionRepeatingCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  if (input_pos_ < static_cast<int>(input_packets_.size())) {
    return CopyReadData(buf, buf_len);
  } else {
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeDatagramSocket::Send(const scoped_refptr<net::IOBuffer>& buf,
                             int buf_len,
                             const net::CompletionRepeatingCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  EXPECT_FALSE(send_pending_);

  if (async_send_) {
    send_pending_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDatagramSocket::DoAsyncSend,
                       weak_factory_.GetWeakPtr(), buf, buf_len, callback));
    return net::ERR_IO_PENDING;
  } else {
    return DoSend(buf, buf_len);
  }
}

void FakeDatagramSocket::DoAsyncSend(
    const scoped_refptr<net::IOBuffer>& buf,
    int buf_len,
    const net::CompletionRepeatingCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

  EXPECT_TRUE(send_pending_);
  send_pending_ = false;
  callback.Run(DoSend(buf, buf_len));
}

int FakeDatagramSocket::DoSend(const scoped_refptr<net::IOBuffer>& buf,
                               int buf_len) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

  if (next_send_error_ != net::OK) {
    int r = next_send_error_;
    next_send_error_ = net::OK;
    return r;
  }

  written_packets_.push_back(std::string());
  written_packets_.back().assign(buf->data(), buf->data() + buf_len);

  if (peer_socket_.get()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDatagramSocket::AppendInputPacket, peer_socket_,
                       std::string(buf->data(), buf->data() + buf_len)));
  }

  return buf_len;
}

int FakeDatagramSocket::CopyReadData(const scoped_refptr<net::IOBuffer>& buf,
                                     int buf_len) {
  int size = std::min(
      buf_len, static_cast<int>(input_packets_[input_pos_].size()));
  memcpy(buf->data(), &(*input_packets_[input_pos_].begin()), size);
  ++input_pos_;
  return size;
}

FakeDatagramChannelFactory::FakeDatagramChannelFactory()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
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
    const ChannelCreatedCallback& callback) {
  EXPECT_FALSE(channels_[name]);

  std::unique_ptr<FakeDatagramSocket> channel(new FakeDatagramSocket());
  channels_[name] = channel->GetWeakPtr();

  if (peer_factory_) {
    FakeDatagramSocket* peer_socket = peer_factory_->GetFakeChannel(name);
    if (peer_socket)
      channel->PairWith(peer_socket);
  }

  if (fail_create_)
    channel.reset();

  if (asynchronous_create_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeDatagramChannelFactory::NotifyChannelCreated,
                       weak_factory_.GetWeakPtr(), std::move(channel), name,
                       callback));
  } else {
    NotifyChannelCreated(std::move(channel), name, callback);
  }
}

void FakeDatagramChannelFactory::NotifyChannelCreated(
    std::unique_ptr<FakeDatagramSocket> owned_socket,
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  if (channels_.find(name) != channels_.end())
    callback.Run(std::move(owned_socket));
}

void FakeDatagramChannelFactory::CancelChannelCreation(
    const std::string& name) {
  channels_.erase(name);
}

}  // namespace protocol
}  // namespace remoting
