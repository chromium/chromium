// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/socket_reader.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace remoting {

namespace {
int kReadBufferSize = 4096;
}  // namespace

SocketReader::SocketReader() : socket_(nullptr) {}

SocketReader::~SocketReader() = default;

void SocketReader::Init(net::Socket* socket,
                        ReadResultCallback read_result_callback) {
  DCHECK(socket);
  DCHECK(!socket_);

  socket_ = socket;
  read_result_callback_ = read_result_callback;
  DoRead();
}

void SocketReader::DoRead() {
  while (true) {
    read_buffer_ = base::MakeRefCounted<net::IOBuffer>(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_.get(),
        kReadBufferSize,
        base::Bind(&SocketReader::OnRead, weak_factory_.GetWeakPtr()));
    HandleReadResult(result);
    if (result <= 0)
      break;
  }
}

void SocketReader::OnRead(int result) {
  HandleReadResult(result);
  if (result > 0)
    DoRead();
}

void SocketReader::HandleReadResult(int result) {
  if (result != net::ERR_IO_PENDING) {
    if (result < 0)
      read_buffer_.reset();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SocketReader::CallCallback, weak_factory_.GetWeakPtr(),
                       read_buffer_, result));
  }
}

void SocketReader::CallCallback(scoped_refptr<net::IOBuffer> data, int result) {
  read_result_callback_.Run(data, result);
}

}  // namespace remoting
