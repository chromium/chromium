// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mojo_socket_test_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TestSocketObserver::TestSocketObserver() = default;

TestSocketObserver::~TestSocketObserver() {
  EXPECT_EQ(net::OK, read_error_);
  EXPECT_EQ(net::OK, write_error_);
}

mojo::PendingRemote<mojom::SocketObserver>
TestSocketObserver::GetObserverRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

int TestSocketObserver::WaitForReadError() {
  read_loop_.Run();
  int error = read_error_;
  read_error_ = net::OK;
  return error;
}

int TestSocketObserver::WaitForWriteError() {
  write_loop_.Run();
  int error = write_error_;
  write_error_ = net::OK;
  return error;
}

void TestSocketObserver::OnReadError(int net_error) {
  read_error_ = net_error;
  read_loop_.Quit();
}

void TestSocketObserver::OnWriteError(int net_error) {
  write_error_ = net_error;
  write_loop_.Quit();
}

}  // namespace network
