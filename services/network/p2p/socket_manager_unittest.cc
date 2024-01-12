// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/p2p/socket_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;

namespace network {

class P2PSocketManagerTest : public testing::Test {
 protected:
  P2PSocketManagerTest() = default;

  void SetUpSocketManager() {
    mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client;
    mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
        trusted_socket_manager;
    auto client_receiver = client.InitWithNewPipeAndPassReceiver();
    auto trusted_socket_manager_remote =
        trusted_socket_manager.InitWithNewPipeAndPassRemote();

    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(
        &mock_socket_factory_);
    url_request_context_ = context_builder->Build();

    socket_manager_ = std::make_unique<P2PSocketManager>(
        net::NetworkAnonymizationKey(), std::move(client),
        std::move(trusted_socket_manager),
        socket_manager_remote_.BindNewPipeAndPassReceiver(),
        base::DoNothingAs<void(P2PSocketManager*)>(),
        url_request_context_.get());
  }

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<P2PSocketManager> socket_manager_;
  mojo::Remote<mojom::P2PSocketManager> socket_manager_remote_;

  base::test::TaskEnvironment task_environment_;
  net::MockClientSocketFactory mock_socket_factory_;
};

// Test to make sure DoGetNetworkList eventually runs SendNetworkList.
TEST_F(P2PSocketManagerTest, DoGetNetworkListTest) {
  net::StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  net::StaticSocketDataProvider socket_data2;
  socket_data2.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  mock_socket_factory_.AddSocketDataProvider(&socket_data1);
  mock_socket_factory_.AddSocketDataProvider(&socket_data2);
  SetUpSocketManager();

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::P2PNetworkNotificationClient> notification_client;
  std::unique_ptr<FakeNetworkNotificationClient> fake_notification_client =
      std::make_unique<FakeNetworkNotificationClient>(
          run_loop.QuitClosure(),
          notification_client.InitWithNewPipeAndPassReceiver());

  socket_manager_remote_->StartNetworkNotifications(
      std::move(notification_client));

  run_loop.Run();

  EXPECT_TRUE(fake_notification_client->get_network_list_changed());
}

}  // namespace network
