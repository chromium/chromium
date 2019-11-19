// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MOJO_SOCKET_TEST_UTIL_H_
#define SERVICES_NETWORK_MOJO_SOCKET_TEST_UTIL_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace network {

// A mojom::SocketObserver implementation used in tests.
class TestSocketObserver : public mojom::SocketObserver {
 public:
  TestSocketObserver();
  ~TestSocketObserver() override;

  // Returns a mojo pending remote. This can only be called once.
  mojo::PendingRemote<mojom::SocketObserver> GetObserverRemote();

  // Waits for Read and Write error. Returns the error observed.
  int WaitForReadError();
  int WaitForWriteError();

 private:
  // mojom::SocketObserver implementation.
  void OnReadError(int net_error) override;
  void OnWriteError(int net_error) override;

  int read_error_ = net::OK;
  int write_error_ = net::OK;
  base::RunLoop read_loop_;
  base::RunLoop write_loop_;
  mojo::Receiver<mojom::SocketObserver> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestSocketObserver);
};

}  // namespace network

#endif  // SERVICES_NETWORK_MOJO_SOCKET_TEST_UTIL_H_
