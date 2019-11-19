// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_stream_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

FakeStreamSocket::FakeStreamSocket()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeStreamSocket::~FakeStreamSocket() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  if (peer_socket_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeStreamSocket::SetReadError, peer_socket_,
                                  net::ERR_CONNECTION_CLOSED));
  }
}

void FakeStreamSocket::AppendInputData(const std::string& data) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  input_data_.insert(input_data_.end(), data.begin(), data.end());
  // Complete pending read if any.
  if (!read_callback_.is_null()) {
    int result = std::min(read_buffer_size_,
                          static_cast<int>(input_data_.size() - input_pos_));
    EXPECT_GT(result, 0);
    memcpy(read_buffer_->data(),
           &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    read_buffer_ = nullptr;

    std::move(read_callback_).Run(result);
  }
}

void FakeStreamSocket::SetReadError(int error) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  // Complete pending read if any.
  if (!read_callback_.is_null()) {
    std::move(read_callback_).Run(error);
  } else {
    next_read_error_ = error;
  }
}

void FakeStreamSocket::PairWith(FakeStreamSocket* peer_socket) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  peer_socket_ = peer_socket->GetWeakPtr();
  peer_socket->peer_socket_ = GetWeakPtr();
}

base::WeakPtr<FakeStreamSocket> FakeStreamSocket::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int FakeStreamSocket::Read(const scoped_refptr<net::IOBuffer>& buf,
                           int buf_len,
                           net::CompletionOnceCallback callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

  if (input_pos_ < static_cast<int>(input_data_.size())) {
    int result = std::min(buf_len,
                          static_cast<int>(input_data_.size()) - input_pos_);
    memcpy(buf->data(), &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    return result;
  } else if (next_read_error_.has_value()) {
    int r = next_read_error_.value();
    next_read_error_.reset();
    return r;
  } else {
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }
}

int FakeStreamSocket::Write(
    const scoped_refptr<net::IOBuffer>& buf,
    int buf_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  EXPECT_FALSE(write_pending_);

  if (write_limit_ > 0)
    buf_len = std::min(write_limit_, buf_len);

  if (async_write_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeStreamSocket::DoAsyncWrite,
                                  weak_factory_.GetWeakPtr(),
                                  scoped_refptr<net::IOBuffer>(buf), buf_len,
                                  std::move(callback)));
    write_pending_ = true;
    return net::ERR_IO_PENDING;
  } else {
    if (next_write_error_ != net::OK) {
      int r = next_write_error_;
      next_write_error_ = net::OK;
      return r;
    }

    DoWrite(buf, buf_len);
    return buf_len;
  }
}

void FakeStreamSocket::DoAsyncWrite(const scoped_refptr<net::IOBuffer>& buf,
                                    int buf_len,
                                    net::CompletionOnceCallback callback) {
  write_pending_ = false;

  if (next_write_error_ != net::OK) {
    int r = next_write_error_;
    next_write_error_ = net::OK;
    std::move(callback).Run(r);
    return;
  }

  DoWrite(buf.get(), buf_len);
  std::move(callback).Run(buf_len);
}

void FakeStreamSocket::DoWrite(const scoped_refptr<net::IOBuffer>& buf,
                               int buf_len) {
  written_data_.insert(written_data_.end(),
                       buf->data(), buf->data() + buf_len);

  if (peer_socket_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeStreamSocket::AppendInputData, peer_socket_,
                       std::string(buf->data(), buf->data() + buf_len)));
  }
}

FakeStreamChannelFactory::FakeStreamChannelFactory()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeStreamChannelFactory::~FakeStreamChannelFactory() = default;

FakeStreamSocket* FakeStreamChannelFactory::GetFakeChannel(
    const std::string& name) {
  return channels_[name].get();
}

void FakeStreamChannelFactory::PairWith(
    FakeStreamChannelFactory* peer_factory) {
  peer_factory_ = peer_factory->weak_factory_.GetWeakPtr();
  peer_factory->peer_factory_ = weak_factory_.GetWeakPtr();
}

void FakeStreamChannelFactory::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  std::unique_ptr<FakeStreamSocket> channel(new FakeStreamSocket());
  channels_[name] = channel->GetWeakPtr();
  channel->set_async_write(async_write_);

  if (peer_factory_) {
    FakeStreamSocket* peer_channel = peer_factory_->GetFakeChannel(name);
    if (peer_channel)
      channel->PairWith(peer_channel);
  }

  if (fail_create_)
    channel.reset();

  if (asynchronous_create_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeStreamChannelFactory::NotifyChannelCreated,
                       weak_factory_.GetWeakPtr(), std::move(channel), name,
                       callback));
  } else {
    NotifyChannelCreated(std::move(channel), name, callback);
  }
}

void FakeStreamChannelFactory::NotifyChannelCreated(
    std::unique_ptr<FakeStreamSocket> owned_channel,
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  if (channels_.find(name) != channels_.end())
    callback.Run(std::move(owned_channel));
}

void FakeStreamChannelFactory::CancelChannelCreation(const std::string& name) {
  channels_.erase(name);
}

}  // namespace protocol
}  // namespace remoting
