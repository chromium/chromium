// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/fuzzed_socket.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/log/net_log_source_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

const int kMaxAsyncReadsAndWrites = 1000;

// Some of the socket errors that can be returned by normal socket connection
// attempts.
const Error kConnectErrors[] = {
    ERR_CONNECTION_RESET,     ERR_CONNECTION_CLOSED, ERR_FAILED,
    ERR_CONNECTION_TIMED_OUT, ERR_ACCESS_DENIED,     ERR_CONNECTION_REFUSED,
    ERR_ADDRESS_UNREACHABLE};

// Some of the socket errors that can be returned by normal socket reads /
// writes. The first one is returned when no more input data remains, so it's
// one of the most common ones.
const Error kReadWriteErrors[] = {ERR_CONNECTION_CLOSED, ERR_FAILED,
                                  ERR_TIMED_OUT, ERR_CONNECTION_RESET};

}  // namespace

FuzzedSocket::FuzzedSocket(FuzzedDataProvider* data_provider,
                           net::NetLog* net_log)
    : data_provider_(data_provider),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)),
      remote_address_(IPEndPoint(IPAddress::IPv4Localhost(), 80)) {}

FuzzedSocket::~FuzzedSocket() = default;

int FuzzedSocket::Read(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) {
  DCHECK(!connect_pending_);
  DCHECK(!read_pending_);

  bool sync;
  int result;

  if (net_error_ != OK) {
    // If an error has already been generated, use it to determine what to do.
    result = net_error_;
    sync = !error_pending_;
  } else {
    // Otherwise, use |data_provider_|. Always consume a bool, even when
    // ForceSync() is true, to behave more consistently against input mutations.
    sync = data_provider_->ConsumeBool() || ForceSync();

    num_async_reads_and_writes_ += static_cast<int>(!sync);

    std::string data = data_provider_->ConsumeRandomLengthString(buf_len);
    result = data.size();

    if (result > 0) {
      std::copy(data.data(), data.data() + result, buf->data());
    } else {
      result = ConsumeReadWriteErrorFromData();
      net_error_ = result;
      if (!sync)
        error_pending_ = true;
    }
  }

  // Graceful close of a socket returns OK, at least in theory. This doesn't
  // perfectly reflect real socket behavior, but close enough.
  if (result == ERR_CONNECTION_CLOSED)
    result = 0;

  if (sync) {
    if (result > 0)
      total_bytes_read_ += result;
    return result;
  }

  read_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FuzzedSocket::OnReadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), result));
  return ERR_IO_PENDING;
}

int FuzzedSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& /* traffic_annotation */) {
  DCHECK(!connect_pending_);
  DCHECK(!write_pending_);

  bool sync;
  int result;

  if (net_error_ != OK) {
    // If an error has already been generated, use it to determine what to do.
    result = net_error_;
    sync = !error_pending_;
  } else {
    // Otherwise, use |data_provider_|. Always consume a bool, even when
    // ForceSync() is true, to behave more consistently against input mutations.
    sync = data_provider_->ConsumeBool() || ForceSync();

    num_async_reads_and_writes_ += static_cast<int>(!sync);

    // Intentionally using smaller |result| size here.
    result = data_provider_->ConsumeIntegralInRange<int>(0, 0xFF);
    if (result > buf_len)
      result = buf_len;
    if (result == 0) {
      net_error_ = ConsumeReadWriteErrorFromData();
      result = net_error_;
      if (!sync)
        error_pending_ = true;
    }
  }

  if (sync) {
    if (result > 0)
      total_bytes_written_ += result;
    return result;
  }

  write_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FuzzedSocket::OnWriteComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), result));
  return ERR_IO_PENDING;
}

int FuzzedSocket::SetReceiveBufferSize(int32_t size) {
  return OK;
}

int FuzzedSocket::SetSendBufferSize(int32_t size) {
  return OK;
}

int FuzzedSocket::Bind(const net::IPEndPoint& local_addr) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int FuzzedSocket::Connect(CompletionOnceCallback callback) {
  // Sockets can normally be reused, but don't support it here.
  DCHECK_NE(net_error_, OK);
  DCHECK(!connect_pending_);
  DCHECK(!read_pending_);
  DCHECK(!write_pending_);
  DCHECK(!error_pending_);
  DCHECK(!total_bytes_read_);
  DCHECK(!total_bytes_written_);

  bool sync = true;
  Error result = OK;
  if (fuzz_connect_result_) {
    // Decide if sync or async. Use async, if no data is left.
    sync = data_provider_->ConsumeBool();
    // Decide if the connect succeeds or not, and if so, pick an error code.
    if (data_provider_->ConsumeBool())
      result = data_provider_->PickValueInArray(kConnectErrors);
  }

  if (sync) {
    net_error_ = result;
    return result;
  }

  connect_pending_ = true;
  if (result != OK)
    error_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FuzzedSocket::OnConnectComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), result));
  return ERR_IO_PENDING;
}

void FuzzedSocket::Disconnect() {
  net_error_ = ERR_CONNECTION_CLOSED;
  weak_factory_.InvalidateWeakPtrs();
  connect_pending_ = false;
  read_pending_ = false;
  write_pending_ = false;
  error_pending_ = false;
}

bool FuzzedSocket::IsConnected() const {
  return net_error_ == OK && !error_pending_;
}

bool FuzzedSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

int FuzzedSocket::GetPeerAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = remote_address_;
  return OK;
}

int FuzzedSocket::GetLocalAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = IPEndPoint(IPAddress(127, 0, 0, 1), 43434);
  return OK;
}

const NetLogWithSource& FuzzedSocket::NetLog() const {
  return net_log_;
}

bool FuzzedSocket::WasEverUsed() const {
  return total_bytes_written_ != 0 || total_bytes_read_ != 0;
}

bool FuzzedSocket::WasAlpnNegotiated() const {
  return false;
}

NextProto FuzzedSocket::GetNegotiatedProtocol() const {
  return kProtoUnknown;
}

bool FuzzedSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

void FuzzedSocket::GetConnectionAttempts(ConnectionAttempts* out) const {
  out->clear();
}

void FuzzedSocket::ClearConnectionAttempts() {}

void FuzzedSocket::AddConnectionAttempts(const ConnectionAttempts& attempts) {}

int64_t FuzzedSocket::GetTotalReceivedBytes() const {
  return total_bytes_read_;
}

void FuzzedSocket::ApplySocketTag(const net::SocketTag& tag) {}

Error FuzzedSocket::ConsumeReadWriteErrorFromData() {
  return data_provider_->PickValueInArray(kReadWriteErrors);
}

void FuzzedSocket::OnReadComplete(CompletionOnceCallback callback, int result) {
  CHECK(read_pending_);
  read_pending_ = false;
  if (result <= 0) {
    error_pending_ = false;
  } else {
    total_bytes_read_ += result;
  }
  std::move(callback).Run(result);
}

void FuzzedSocket::OnWriteComplete(CompletionOnceCallback callback,
                                   int result) {
  CHECK(write_pending_);
  write_pending_ = false;
  if (result <= 0) {
    error_pending_ = false;
  } else {
    total_bytes_written_ += result;
  }
  std::move(callback).Run(result);
}

void FuzzedSocket::OnConnectComplete(CompletionOnceCallback callback,
                                     int result) {
  CHECK(connect_pending_);
  connect_pending_ = false;
  if (result < 0)
    error_pending_ = false;
  net_error_ = result;
  std::move(callback).Run(result);
}

bool FuzzedSocket::ForceSync() const {
  return (num_async_reads_and_writes_ >= kMaxAsyncReadsAndWrites);
}

}  // namespace net
