// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "socket_broker_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_descriptor.h"
#include "net/test/gtest_util.h"
#include "services/network/public/mojom/socket_broker.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

void DidCompleteCreateTest(base::RunLoop* run_loop,
                           network::TransferableSocket socket,
                           int rv) {
  EXPECT_NE(socket.TakeSocket(), net::kInvalidSocket);
  EXPECT_EQ(rv, net::OK);

  run_loop->Quit();
}

TEST(SocketBrokerImplTest, TestCanOpenSocket) {
  content::BrowserTaskEnvironment task_environment;

  SocketBrokerImpl socket_broker_impl;
  base::RunLoop run_loop;
  mojo::Remote<network::mojom::SocketBroker> remote(
      socket_broker_impl.BindNewRemote());
  remote->CreateTcpSocket(net::ADDRESS_FAMILY_IPV4,
                          base::BindOnce(&DidCompleteCreateTest, &run_loop));
  run_loop.Run();

  base::RunLoop run_loop2;
  remote->CreateTcpSocket(net::ADDRESS_FAMILY_IPV6,
                          base::BindOnce(&DidCompleteCreateTest, &run_loop2));
  run_loop2.Run();
}

}  // namespace content
