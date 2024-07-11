// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_test_util.h"

#include "net/base/completion_once_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

FakeStreamSocket::FakeStreamSocket() : MockClientSocket(NetLogWithSource()) {
  connected_ = true;
}

FakeStreamSocket::~FakeStreamSocket() = default;

int FakeStreamSocket::Read(IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  return 0;
}

int FakeStreamSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return 0;
}

int FakeStreamSocket::Connect(CompletionOnceCallback callback) {
  return OK;
}

bool FakeStreamSocket::IsConnected() const {
  return connected_;
}

bool FakeStreamSocket::IsConnectedAndIdle() const {
  return connected_ && is_idle_;
}

bool FakeStreamSocket::WasEverUsed() const {
  return was_ever_used_;
}

bool FakeStreamSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

}  // namespace net
