// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_
#define NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_

#include "net/base/completion_once_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class IOBuffer;
class SSLInfo;

class FakeStreamSocket : public MockClientSocket {
 public:
  FakeStreamSocket();

  FakeStreamSocket(const FakeStreamSocket&) = delete;
  FakeStreamSocket& operator=(const FakeStreamSocket&) = delete;

  ~FakeStreamSocket() override;

  void set_is_connected(bool connected) { connected_ = connected; }

  void set_is_idle(bool is_idle) { is_idle_ = is_idle; }

  void set_was_ever_used(bool was_ever_used) { was_ever_used_ = was_ever_used; }

  void set_peer_addr(IPEndPoint peer_addr) { peer_addr_ = peer_addr; }

  // StreamSocket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int Connect(CompletionOnceCallback callback) override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  bool WasEverUsed() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;

 private:
  bool is_idle_ = true;
  bool was_ever_used_ = false;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_
