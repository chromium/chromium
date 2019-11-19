// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/fuzzed_datagram_client_socket.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// Subset of network errors that can occur on each operation. Less clear cut
// than TCP errors, so some of these may not actually be possible.
const Error kConnectErrors[] = {ERR_FAILED, ERR_ADDRESS_UNREACHABLE,
                                ERR_ACCESS_DENIED};
const Error kReadErrors[] = {ERR_FAILED, ERR_ADDRESS_UNREACHABLE};
const Error kWriteErrors[] = {ERR_FAILED, ERR_ADDRESS_UNREACHABLE,
                              ERR_MSG_TOO_BIG};

FuzzedDatagramClientSocket::FuzzedDatagramClientSocket(
    FuzzedDataProvider* data_provider)
    : data_provider_(data_provider) {}

FuzzedDatagramClientSocket::~FuzzedDatagramClientSocket() = default;

int FuzzedDatagramClientSocket::Connect(const IPEndPoint& address) {
  CHECK(!connected_);

  // Decide if the connect attempt succeeds.
  if (data_provider_->ConsumeBool()) {
    connected_ = true;
    remote_address_ = address;
    return OK;
  }

  // On failure, return a random connect error.
  return data_provider_->PickValueInArray(kConnectErrors);
}

int FuzzedDatagramClientSocket::ConnectUsingNetwork(
    NetworkChangeNotifier::NetworkHandle network,
    const IPEndPoint& address) {
  CHECK(!connected_);
  return ERR_NOT_IMPLEMENTED;
}

int FuzzedDatagramClientSocket::FuzzedDatagramClientSocket::
    ConnectUsingDefaultNetwork(const IPEndPoint& address) {
  CHECK(!connected_);
  return ERR_NOT_IMPLEMENTED;
}

NetworkChangeNotifier::NetworkHandle
FuzzedDatagramClientSocket::GetBoundNetwork() const {
  return NetworkChangeNotifier::kInvalidNetworkHandle;
}

void FuzzedDatagramClientSocket::ApplySocketTag(const SocketTag& tag) {}

void FuzzedDatagramClientSocket::Close() {
  connected_ = false;
  read_pending_ = false;
  write_pending_ = false;
  remote_address_ = IPEndPoint();
  weak_factory_.InvalidateWeakPtrs();
}

int FuzzedDatagramClientSocket::GetPeerAddress(IPEndPoint* address) const {
  if (!connected_)
    return ERR_SOCKET_NOT_CONNECTED;
  *address = remote_address_;
  return OK;
}

int FuzzedDatagramClientSocket::GetLocalAddress(IPEndPoint* address) const {
  if (!connected_)
    return ERR_SOCKET_NOT_CONNECTED;
  *address = IPEndPoint(IPAddress(1, 2, 3, 4), 43210);
  return OK;
}

void FuzzedDatagramClientSocket::UseNonBlockingIO() {}

int FuzzedDatagramClientSocket::WriteAsync(
    DatagramBuffers buffers,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return -1;
}

int FuzzedDatagramClientSocket::WriteAsync(
    const char* buffer,
    size_t buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return -1;
}

DatagramBuffers FuzzedDatagramClientSocket::GetUnwrittenBuffers() {
  DatagramBuffers result;
  return result;
}

void FuzzedDatagramClientSocket::SetWriteAsyncEnabled(bool enabled) {}
bool FuzzedDatagramClientSocket::WriteAsyncEnabled() {
  return false;
}
void FuzzedDatagramClientSocket::SetMaxPacketSize(size_t max_packet_size) {}
void FuzzedDatagramClientSocket::SetWriteMultiCoreEnabled(bool enabled) {}
void FuzzedDatagramClientSocket::SetSendmmsgEnabled(bool enabled) {}
void FuzzedDatagramClientSocket::SetWriteBatchingActive(bool active) {}
int FuzzedDatagramClientSocket::SetMulticastInterface(
    uint32_t interface_index) {
  return ERR_NOT_IMPLEMENTED;
}

const NetLogWithSource& FuzzedDatagramClientSocket::NetLog() const {
  return net_log_;
}

int FuzzedDatagramClientSocket::Read(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  CHECK(!callback.is_null());
  CHECK_GT(buf_len, 0);
  CHECK(!read_pending_);

  // Normally calling this on disconnected sockets is allowed, but code really
  // shouldn't be doing this.  If it is, it's best to figure out why, and fix
  // it. Note that |connected_| is only set to false on calls to Close(), not on
  // errors.
  CHECK(connected_);

  // Get contents of response.
  std::string data = data_provider_->ConsumeRandomLengthString(
      data_provider_->ConsumeIntegralInRange(0, buf_len));

  int result;
  if (data.size() > 0) {
    // If the response is not empty, consider it a successful read.
    result = data.size();
    std::copy(data.begin(), data.end(), buf->data());
  } else {
    // If the response is empty, pick a random read error.
    result = data_provider_->PickValueInArray(kReadErrors);
  }

  // Decide if result should be returned synchronously.
  if (data_provider_->ConsumeBool())
    return result;

  read_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FuzzedDatagramClientSocket::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), result));
  return ERR_IO_PENDING;
}

int FuzzedDatagramClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& /* traffic_annotation */) {
  CHECK(!callback.is_null());
  CHECK(!write_pending_);

  // Normally this is allowed, but code really shouldn't be doing this - if it
  // is, it's best to figure out why, and fix it.
  CHECK(connected_);

  int result;
  // Decide if success or failure.
  if (data_provider_->ConsumeBool()) {
    // On success, everything is written.
    result = buf_len;
  } else {
    // On failure, pick a random write error.
    result = data_provider_->PickValueInArray(kWriteErrors);
  }

  // Decide if result should be returned synchronously.
  if (data_provider_->ConsumeBool())
    return result;

  write_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FuzzedDatagramClientSocket::OnWriteComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), result));
  return ERR_IO_PENDING;
}

int FuzzedDatagramClientSocket::SetReceiveBufferSize(int32_t size) {
  return OK;
}

int FuzzedDatagramClientSocket::SetSendBufferSize(int32_t size) {
  return OK;
}

int FuzzedDatagramClientSocket::SetDoNotFragment() {
  return OK;
}

void FuzzedDatagramClientSocket::OnReadComplete(
    net::CompletionOnceCallback callback,
    int result) {
  CHECK(connected_);
  CHECK(read_pending_);

  read_pending_ = false;
  std::move(callback).Run(result);
}

void FuzzedDatagramClientSocket::OnWriteComplete(
    net::CompletionOnceCallback callback,
    int result) {
  CHECK(connected_);
  CHECK(write_pending_);

  write_pending_ = false;
  std::move(callback).Run(result);
}

}  // namespace net
