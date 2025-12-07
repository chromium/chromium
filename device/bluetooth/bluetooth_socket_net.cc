// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_net.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

const char kSocketNotConnected[] = "Socket is not connected.";

static void DeactivateSocket(
    const scoped_refptr<device::BluetoothSocketThread>& socket_thread) {
  socket_thread->OnSocketDeactivate();
}

}  // namespace

namespace device {

BluetoothSocketNet::WriteRequest::WriteRequest()
    : buffer_size(0) {}

BluetoothSocketNet::WriteRequest::~WriteRequest() = default;

BluetoothSocketNet::BluetoothSocketNet(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread)
    : ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread) {
  DCHECK(ui_task_runner->RunsTasksInCurrentSequence());
  socket_thread_->OnSocketActivate();
}

BluetoothSocketNet::~BluetoothSocketNet() {
  DCHECK(!tcp_socket_);
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&DeactivateSocket, socket_thread_));
}

void BluetoothSocketNet::Disconnect(base::OnceClosure success_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketNet::DoDisconnect, this,
                     base::BindOnce(&BluetoothSocketNet::PostSuccess, this,
                                    std::move(success_callback))));
}

void BluetoothSocketNet::Receive(
    int buffer_size,
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BluetoothSocketNet::DoReceive, this, buffer_size,
          base::BindOnce(&BluetoothSocketNet::PostReceiveCompletion, this,
                         std::move(success_callback)),
          base::BindOnce(&BluetoothSocketNet::PostReceiveErrorCompletion, this,
                         std::move(error_callback))));
}

void BluetoothSocketNet::Send(scoped_refptr<net::IOBuffer> buffer,
                              int buffer_size,
                              SendCompletionCallback success_callback,
                              ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketNet::DoSend, this, buffer, buffer_size,
                     base::BindOnce(&BluetoothSocketNet::PostSendCompletion,
                                    this, std::move(success_callback)),
                     base::BindOnce(&BluetoothSocketNet::PostErrorCompletion,
                                    this, std::move(error_callback))));
}

void BluetoothSocketNet::ResetData() {
}

void BluetoothSocketNet::ResetTCPSocket() {
  tcp_socket_ = net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
}

void BluetoothSocketNet::SetTCPSocket(
    std::unique_ptr<net::TCPSocket> tcp_socket) {
  tcp_socket_ = std::move(tcp_socket);
}

void BluetoothSocketNet::PostSuccess(base::OnceClosure callback) {
  ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
}

void BluetoothSocketNet::PostErrorCompletion(ErrorCompletionCallback callback,
                                             const std::string& error) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), error));
}

void BluetoothSocketNet::DoDisconnect(base::OnceClosure callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (tcp_socket_) {
    tcp_socket_->Close();
    tcp_socket_.reset(NULL);
  }

  // Note: Closing |tcp_socket_| above released all potential pending
  // Send/Receive operations, so we can no safely release the state associated
  // to those pending operations.
  read_buffer_.reset();
  base::queue<std::unique_ptr<WriteRequest>> empty;
  std::swap(write_queue_, empty);

  ResetData();
  std::move(callback).Run();
}

void BluetoothSocketNet::DoReceive(
    int buffer_size,
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!tcp_socket_) {
    std::move(error_callback)
        .Run(BluetoothSocket::kDisconnected, kSocketNotConnected);
    return;
  }

  // Only one pending read at a time
  if (read_buffer_.get()) {
    std::move(error_callback)
        .Run(BluetoothSocket::kIOPending,
             net::ErrorToString(net::ERR_IO_PENDING));
    return;
  }

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&BluetoothSocketNet::OnSocketReadComplete, this,
                     std::move(success_callback), std::move(error_callback)));
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(buffer_size);
  int read_result = tcp_socket_->Read(buffer.get(), buffer->size(),
                                      std::move(split_callback.first));

  read_buffer_ = buffer;

  // Read() will not have run |split_callback.first| if there is no pending I/O.
  if (read_result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(read_result);
  }
}

void BluetoothSocketNet::OnSocketReadComplete(
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback,
    int read_result) {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());

  scoped_refptr<net::IOBufferWithSize> buffer;
  buffer.swap(read_buffer_);
  if (read_result > 0) {
    std::move(success_callback).Run(read_result, buffer);
  } else if (read_result == net::OK ||
             read_result == net::ERR_CONNECTION_CLOSED ||
             read_result == net::ERR_CONNECTION_RESET) {
    std::move(error_callback)
        .Run(BluetoothSocket::kDisconnected, net::ErrorToString(read_result));
  } else {
    std::move(error_callback)
        .Run(BluetoothSocket::kSystemError, net::ErrorToString(read_result));
  }
}

void BluetoothSocketNet::DoSend(scoped_refptr<net::IOBuffer> buffer,
                                int buffer_size,
                                SendCompletionCallback success_callback,
                                ErrorCompletionCallback error_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());

  if (!tcp_socket_) {
    std::move(error_callback).Run(kSocketNotConnected);
    return;
  }

  auto request = std::make_unique<WriteRequest>();
  request->buffer = buffer;
  request->buffer_size = buffer_size;
  request->success_callback = std::move(success_callback);
  request->error_callback = std::move(error_callback);

  write_queue_.push(std::move(request));
  if (write_queue_.size() == 1) {
    SendFrontWriteRequest();
  }
}

void BluetoothSocketNet::SendFrontWriteRequest() {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!tcp_socket_)
    return;

  if (pending_write_request_) {
    // It is possible to enter this function while a write request is
    // currently pending if the following sequence happens:
    //
    // 1) A single pending write is running and it is the last one in the queue.
    // 2) A Send() call queues a DoSend() call on the sequence.
    // 3) The pending write completes, queue length is zero, a call to
    //      SendFrontWriteRequest is queued on the sequence.
    // 4) DoSend() runs on the sequence, queues a write request, and runs
    //      SendFrontWriteRequest() inline because the queue size is 1.
    // 5) The immediate call for SendFrontWriteRequest() starts a write request
    //      and exits.
    // 6) The next SendFrontWriteRequest() which was queued in step 3 now runs
    //      while the write request from 5 is still pending.
    //
    // At this point we have entered SendFrontWriteRequest() while we are
    // waiting for a pending write. Previously the code did not handle this
    // situation and would attempt to process the write request at the front
    // of the queue twice which is both wrong from a data perspective and also
    // triggers a CHECK in the Socket.
    //
    // The fix is to ensure we only process a new write request when there are
    // no pending requests, so we exit early here and let OnSocketWriteComplete
    // queue the next SendFrontWriteRequest.
    return;
  }

  if (write_queue_.size() == 0)
    return;

  pending_write_request_ = std::move(write_queue_.front());
  write_queue_.pop();

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&BluetoothSocketNet::OnSocketWriteComplete, this,
                     std::move(pending_write_request_->success_callback),
                     std::move(pending_write_request_->error_callback)));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("bluetooth_socket", R"(
        semantics {
          sender: "Bluetooth Socket"
          description:
            "This socket connects to a bluetooth device for local data "
            "transfer."
          trigger:
            "When user selects to connect to a bluetooth device or communicate "
            "with it."
          data:
            "Any data that needs to be sent to a bluetooth device."
          destination: OTHER
          destination_other: "Data is sent to a bluetooth device."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but it will not be "
            "used if bluetooth connections are not made."
          policy_exception_justification:
            "DeviceAllowBluetooth policy can disable Bluetooth for ChromeOS, "
            "not implemented for other platforms."
        })");
  int send_result = tcp_socket_->Write(
      pending_write_request_->buffer.get(), pending_write_request_->buffer_size,
      std::move(split_callback.first), traffic_annotation);

  // Write() will not have run |split_callback.first| if there is no pending
  // I/O.
  if (send_result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(send_result);
  }
}

void BluetoothSocketNet::OnSocketWriteComplete(
    SendCompletionCallback success_callback,
    ErrorCompletionCallback error_callback,
    int send_result) {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());

  pending_write_request_.reset();

  if (send_result >= net::OK) {
    std::move(success_callback).Run(send_result);
  } else {
    std::move(error_callback).Run(net::ErrorToString(send_result));
  }

  // Don't call directly to avoid potentail large recursion.
  socket_thread_->task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketNet::SendFrontWriteRequest, this));
}

void BluetoothSocketNet::PostReceiveCompletion(
    ReceiveCompletionCallback callback,
    int io_buffer_size,
    scoped_refptr<net::IOBuffer> io_buffer) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), io_buffer_size, io_buffer));
}

void BluetoothSocketNet::PostReceiveErrorCompletion(
    ReceiveErrorCompletionCallback callback,
    ErrorReason reason,
    const std::string& error_message) {
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reason, error_message));
}

void BluetoothSocketNet::PostSendCompletion(SendCompletionCallback callback,
                                            int bytes_written) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), bytes_written));
}

}  // namespace device
