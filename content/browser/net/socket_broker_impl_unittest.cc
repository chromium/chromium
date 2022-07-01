// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "socket_broker_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
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
                           mojo::PlatformHandle fd,
                           int rv) {
// TODO(https://crbug.com/1311014): Remove ifdef and expect a result of net::OK
// on Windows.
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(rv, net::ERR_FAILED);
#else
  EXPECT_NE(fd.ReleaseFD(), net::kInvalidSocket);
  EXPECT_EQ(rv, net::OK);
#endif

  run_loop->Quit();
}

TEST(SocketBrokerImplTest, TestCanOpenSocket) {
  SocketBrokerImpl socket_broker_impl;
  base::test::TaskEnvironment task_environment;
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
