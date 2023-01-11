// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/buffered_socket_writer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace remoting {

namespace {

int WriteNetSocket(net::Socket* socket,
                   const scoped_refptr<net::IOBuffer>& buf,
                   int buf_len,
                   net::CompletionOnceCallback callback,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  return socket->Write(buf.get(), buf_len, std::move(callback),
                       traffic_annotation);
}

}  // namespace

struct BufferedSocketWriter::PendingPacket {
  PendingPacket(scoped_refptr<net::DrainableIOBuffer> data,
                base::OnceClosure done_task,
                const net::NetworkTrafficAnnotationTag& traffic_annotation)
      : data(data),
        done_task(std::move(done_task)),
        traffic_annotation(traffic_annotation) {}

  scoped_refptr<net::DrainableIOBuffer> data;
  base::OnceClosure done_task;
  net::NetworkTrafficAnnotationTag traffic_annotation;
};

// static
std::unique_ptr<BufferedSocketWriter> BufferedSocketWriter::CreateForSocket(
    net::Socket* socket,
    WriteFailedCallback write_failed_callback) {
  std::unique_ptr<BufferedSocketWriter> result =
      std::make_unique<BufferedSocketWriter>();
  result->Start(base::BindRepeating(&WriteNetSocket, socket),
                std::move(write_failed_callback));
  return result;
}

BufferedSocketWriter::BufferedSocketWriter() {}

BufferedSocketWriter::~BufferedSocketWriter() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BufferedSocketWriter::Start(const WriteCallback& write_callback,
                                 WriteFailedCallback write_failed_callback) {
  write_callback_ = write_callback;
  write_failed_callback_ = std::move(write_failed_callback);
  DoWrite();
}

void BufferedSocketWriter::Write(
    scoped_refptr<net::IOBufferWithSize> data,
    base::OnceClosure done_task,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data.get());

  // Don't write after error.
  if (closed_) {
    return;
  }

  int data_size = data->size();
  queue_.push_back(std::make_unique<PendingPacket>(
      base::MakeRefCounted<net::DrainableIOBuffer>(std::move(data), data_size),
      std::move(done_task), traffic_annotation));

  DoWrite();
}

void BufferedSocketWriter::DoWrite() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::WeakPtr<BufferedSocketWriter> self = weak_factory_.GetWeakPtr();
  while (self && !write_pending_ && !write_callback_.is_null() &&
         !queue_.empty()) {
    int result = write_callback_.Run(
        queue_.front()->data.get(), queue_.front()->data->BytesRemaining(),
        base::BindOnce(&BufferedSocketWriter::OnWritten,
                       weak_factory_.GetWeakPtr()),
        queue_.front()->traffic_annotation);
    HandleWriteResult(result);
  }
}

void BufferedSocketWriter::HandleWriteResult(int result) {
  if (result < 0) {
    if (result == net::ERR_IO_PENDING) {
      write_pending_ = true;
    } else {
      closed_ = true;
      write_callback_.Reset();
      if (!write_failed_callback_.is_null()) {
        std::move(write_failed_callback_).Run(result);
      }
    }
    return;
  }

  DCHECK(!queue_.empty());

  queue_.front()->data->DidConsume(result);

  if (queue_.front()->data->BytesRemaining() == 0) {
    base::OnceClosure done_task = std::move(queue_.front()->done_task);
    queue_.pop_front();

    if (!done_task.is_null()) {
      std::move(done_task).Run();
    }
  }
}

void BufferedSocketWriter::OnWritten(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(write_pending_);
  write_pending_ = false;

  base::WeakPtr<BufferedSocketWriter> self = weak_factory_.GetWeakPtr();
  HandleWriteResult(result);
  if (self) {
    DoWrite();
  }
}

}  // namespace remoting
