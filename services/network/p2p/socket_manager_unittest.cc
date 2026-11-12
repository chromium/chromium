// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_manager.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/p2p/socket.h"
#include "services/network/p2p/socket_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

namespace network {
namespace {

// HostResolver test double that hangs requests until explicitly completed or
// cancelled. When `fail_siblings_on_cancel` is armed, cancelling one in-flight
// request synchronously completes all remaining pending requests with
// `ERR_NAME_NOT_RESOLVED` inside `~ResolveHostRequest()`, matching the
// production re-entrancy when cancelling the last active DoH stream on a
// stalled socket pool triggers `SpdySession::StartGoingAway()`.
class ReentrantFailingHostResolver : public net::HangingHostResolver {
 public:
  ReentrantFailingHostResolver() = default;
  ~ReentrantFailingHostResolver() override = default;

  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      net::NetworkAnonymizationKey network_anonymization_key,
      net::handles::NetworkHandle target_network,
      net::NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override {
    return CreateRequest(net::HostPortPair::FromSchemeHostPort(host),
                         network_anonymization_key, target_network, net_log,
                         optional_parameters);
  }

  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const net::HostPortPair& host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::handles::NetworkHandle target_network,
      const net::NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters)
      override {
    return std::make_unique<ResolveRequest>(weak_factory_.GetWeakPtr());
  }

  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      net::NetworkAnonymizationKey network_anonymization_key,
      net::handles::NetworkHandle target_network,
      net::NetLogWithSource net_log,
      ResolveHostParameters parameters) override {
    return std::make_unique<EndpointRequest>(weak_factory_.GetWeakPtr());
  }

  bool IsHappyEyeballsV3Enabled() const override { return false; }

  void set_fail_siblings_on_cancel(bool value) {
    fail_siblings_on_cancel_ = value;
  }

  size_t pending_count() const { return pending_requests_.size(); }

  void WaitForPendingRequests(size_t expected_count) {
    while (pending_requests_.size() < expected_count) {
      base::RunLoop run_loop;
      on_request_started_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 private:
  class PendingRequest {
   public:
    virtual ~PendingRequest() = default;
    virtual void CompleteSynchronously(int error) = 0;
  };

  class ResolveRequest : public net::HostResolver::ResolveHostRequest,
                         public PendingRequest {
   public:
    explicit ResolveRequest(
        base::WeakPtr<ReentrantFailingHostResolver> resolver)
        : resolver_(std::move(resolver)) {}

    ResolveRequest(const ResolveRequest&) = delete;
    ResolveRequest& operator=(const ResolveRequest&) = delete;

    ~ResolveRequest() override {
      if (resolver_) {
        resolver_->OnRequestCancelled(this);
      }
    }

    int Start(net::CompletionOnceCallback callback) override {
      callback_ = std::move(callback);
      if (resolver_) {
        resolver_->OnRequestStarted(this);
      }
      return net::ERR_IO_PENDING;
    }

    void CompleteSynchronously(int error) override {
      error_ = error;
      if (callback_) {
        std::move(callback_).Run(error);
      }
    }

    const net::AddressList& GetAddressResults() const override {
      return empty_addresses_;
    }

    base::span<const net::HostResolverEndpointResult> GetEndpointResults()
        const override {
      return {};
    }

    base::span<const std::string> GetTextResults() const override { return {}; }

    base::span<const net::HostPortPair> GetHostnameResults() const override {
      return {};
    }

    const std::set<std::string>& GetDnsAliasResults() const override {
      return empty_aliases_;
    }

    net::ResolveErrorInfo GetResolveErrorInfo() const override {
      return net::ResolveErrorInfo(error_);
    }

    const std::optional<net::HostCache::EntryStaleness>& GetStaleInfo()
        const override {
      return empty_stale_info_;
    }

    std::optional<net::ResolutionDetails> GetResolutionDetails()
        const override {
      return std::nullopt;
    }

   private:
    base::WeakPtr<ReentrantFailingHostResolver> resolver_;
    net::CompletionOnceCallback callback_;
    int error_ = net::ERR_NAME_NOT_RESOLVED;
    net::AddressList empty_addresses_;
    std::set<std::string> empty_aliases_;
    std::optional<net::HostCache::EntryStaleness> empty_stale_info_;
  };

  class EndpointRequest : public net::HostResolver::ServiceEndpointRequest,
                          public PendingRequest {
   public:
    explicit EndpointRequest(
        base::WeakPtr<ReentrantFailingHostResolver> resolver)
        : resolver_(std::move(resolver)) {}

    EndpointRequest(const EndpointRequest&) = delete;
    EndpointRequest& operator=(const EndpointRequest&) = delete;

    ~EndpointRequest() override {
      if (resolver_) {
        resolver_->OnRequestCancelled(this);
      }
    }

    int Start(Delegate* delegate) override {
      CHECK(delegate);
      delegate_ = delegate;
      if (resolver_) {
        resolver_->OnRequestStarted(this);
      }
      return net::ERR_IO_PENDING;
    }

    void CompleteSynchronously(int error) override {
      error_ = error;
      Delegate* delegate = delegate_;
      delegate_ = nullptr;
      if (delegate) {
        delegate->OnServiceEndpointRequestFinished(error);
      }
    }

    base::span<const net::ServiceEndpoint> GetEndpointResults() override {
      return {};
    }

    const std::set<std::string>& GetDnsAliasResults() override {
      return empty_aliases_;
    }

    bool EndpointsCryptoReady() override { return false; }

    net::ResolveErrorInfo GetResolveErrorInfo() override {
      return net::ResolveErrorInfo(error_);
    }

    const net::HostCache::EntryStaleness* GetStaleInfo() const override {
      return nullptr;
    }

    bool IsStaleWhileRefreshing() const override { return false; }

    void ChangeRequestPriority(net::RequestPriority priority) override {}

    std::optional<net::ResolutionDetails> GetResolutionDetails()
        const override {
      return std::nullopt;
    }

   private:
    base::WeakPtr<ReentrantFailingHostResolver> resolver_;
    raw_ptr<Delegate> delegate_ = nullptr;
    int error_ = net::ERR_NAME_NOT_RESOLVED;
    std::set<std::string> empty_aliases_;
  };

  void OnRequestStarted(PendingRequest* request) {
    pending_requests_.push_back(request);
    if (on_request_started_) {
      std::move(on_request_started_).Run();
    }
  }

  void OnRequestCancelled(PendingRequest* request) {
    size_t erased = std::erase(pending_requests_, request);
    if (erased > 0 && fail_siblings_on_cancel_ && !failing_siblings_) {
      base::AutoReset<bool> reset(&failing_siblings_, true);
      while (!pending_requests_.empty()) {
        PendingRequest* sibling = pending_requests_.front().get();
        pending_requests_.erase(pending_requests_.begin());
        sibling->CompleteSynchronously(net::ERR_NAME_NOT_RESOLVED);
      }
    }
  }

  std::vector<raw_ptr<PendingRequest>> pending_requests_;
  base::OnceClosure on_request_started_;
  bool fail_siblings_on_cancel_ = false;
  bool failing_siblings_ = false;
  base::WeakPtrFactory<ReentrantFailingHostResolver> weak_factory_{this};
};

}  // namespace

class P2PSocketManagerTest : public testing::Test {
 protected:
  P2PSocketManagerTest() = default;

  void SetUpSocketManager(
      std::unique_ptr<net::HostResolver> host_resolver = nullptr) {
    mojo::PendingRemote<mojom::P2PTrustedSocketManagerClient> client;
    mojo::PendingReceiver<mojom::P2PTrustedSocketManager>
        trusted_socket_manager;
    trusted_socket_manager_client_receiver_ =
        client.InitWithNewPipeAndPassReceiver();
    trusted_socket_manager_remote_ =
        trusted_socket_manager.InitWithNewPipeAndPassRemote();

    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(
        &mock_socket_factory_);
    if (host_resolver) {
      context_builder->set_host_resolver(std::move(host_resolver));
    }
    url_request_context_ = context_builder->Build();

    socket_manager_ = std::make_unique<P2PSocketManager>(
        net::NetworkAnonymizationKey(), std::move(client),
        std::move(trusted_socket_manager),
        socket_manager_remote_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&P2PSocketManagerTest::OnSocketManagerDestroyed,
                       base::Unretained(this)),
        url_request_context_.get());
  }

  mojo::Remote<mojom::P2PSocket> CreateStunTcpSocket(
      const std::string& hostname) {
    mojo::Remote<mojom::P2PSocket> socket_remote;
    mojo::PendingRemote<mojom::P2PSocketClient> client_remote;
    std::ignore = client_remote.InitWithNewPipeAndPassReceiver();

    P2PPortRange port_range;
    port_range.min_port = 0;
    port_range.max_port = 0;

    P2PHostAndIPEndPoint remote_address;
    remote_address.hostname = hostname;
    remote_address.ip_address =
        net::IPEndPoint(net::IPAddress::IPv4Localhost(), 3478);

    socket_manager_remote_->CreateSocket(
        P2P_SOCKET_STUN_TCP_CLIENT,
        net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), port_range,
        remote_address,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::nullopt, std::move(client_remote),
        socket_remote.BindNewPipeAndPassReceiver());
    return socket_remote;
  }

  void OnSocketManagerDestroyed(P2PSocketManager* socket_manager) {
    ASSERT_EQ(socket_manager, socket_manager_.get());
    socket_manager_.reset();
  }

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<P2PSocketManager> socket_manager_;
  mojo::Remote<mojom::P2PSocketManager> socket_manager_remote_;
  mojo::PendingReceiver<mojom::P2PTrustedSocketManagerClient>
      trusted_socket_manager_client_receiver_;
  mojo::PendingRemote<mojom::P2PTrustedSocketManager>
      trusted_socket_manager_remote_;

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

TEST_F(P2PSocketManagerTest, CreateSocketRestrictedPort) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnforceP2PSocketPortRestrictions);
  SetUpSocketManager();

  mojo::Remote<mojom::P2PSocket> socket_remote;
  mojo::PendingRemote<mojom::P2PSocketClient> client_remote;
  std::ignore = client_remote.InitWithNewPipeAndPassReceiver();

  P2PPortRange port_range;
  port_range.min_port = 0;
  port_range.max_port = 0;

  P2PHostAndIPEndPoint remote_address;
  net::IPAddress ip(192, 168, 1, 10);
  remote_address.ip_address =
      net::IPEndPoint(ip, 25);  // Restricted SMTP port (25).

  mojo::PendingReceiver<mojom::P2PSocket> socket_receiver =
      socket_remote.BindNewPipeAndPassReceiver();
  base::RunLoop run_loop;
  socket_remote.set_disconnect_handler(run_loop.QuitClosure());

  socket_manager_remote_->CreateSocket(
      P2P_SOCKET_STUN_TCP_CLIENT,
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0), port_range,
      remote_address,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      std::nullopt, std::move(client_remote), std::move(socket_receiver));

  run_loop.Run();
  EXPECT_FALSE(socket_remote.is_connected());
}

// Regression test for crbug.com/579289829: destroying a P2P TCP socket inside
// DestroySocket() can synchronously fail sibling sockets and re-enter
// DestroySocket().
TEST_F(P2PSocketManagerTest, DestroySocketHandlesReentrantSiblingFailure) {
  auto resolver = std::make_unique<ReentrantFailingHostResolver>();
  ReentrantFailingHostResolver* resolver_ptr = resolver.get();
  SetUpSocketManager(std::move(resolver));

  constexpr size_t kNumSockets = 4;
  std::vector<mojo::Remote<mojom::P2PSocket>> sockets;
  sockets.reserve(kNumSockets);
  for (size_t i = 0; i < kNumSockets; ++i) {
    sockets.push_back(
        CreateStunTcpSocket("stun" + base::NumberToString(i) + ".example"));
  }
  resolver_ptr->WaitForPendingRequests(kNumSockets);

  base::RunLoop siblings_disconnected_loop;
  base::RepeatingClosure barrier = base::BarrierClosure(
      kNumSockets - 1, siblings_disconnected_loop.QuitClosure());
  for (size_t i = 1; i < kNumSockets; ++i) {
    sockets[i].set_disconnect_handler(barrier);
  }

  resolver_ptr->set_fail_siblings_on_cancel(true);
  // Disconnecting socket 0 triggers DestroySocket(socket0), whose destructor
  // cancels its ResolveHostRequest and synchronously fails sockets 1..3.
  sockets[0].reset();

  siblings_disconnected_loop.Run();
  EXPECT_EQ(resolver_ptr->pending_count(), 0u);
  for (size_t i = 1; i < kNumSockets; ++i) {
    EXPECT_FALSE(sockets[i].is_connected());
  }
}

// Verifies ~P2PSocketManager() safely handles synchronous sibling socket
// failures triggered while tearing down `sockets_`.
TEST_F(P2PSocketManagerTest, DestructorHandlesReentrantSocketFailure) {
  auto resolver = std::make_unique<ReentrantFailingHostResolver>();
  ReentrantFailingHostResolver* resolver_ptr = resolver.get();
  SetUpSocketManager(std::move(resolver));

  constexpr size_t kNumSockets = 4;
  std::vector<mojo::Remote<mojom::P2PSocket>> sockets;
  sockets.reserve(kNumSockets);
  for (size_t i = 0; i < kNumSockets; ++i) {
    sockets.push_back(
        CreateStunTcpSocket("stun" + base::NumberToString(i) + ".example"));
  }
  resolver_ptr->WaitForPendingRequests(kNumSockets);

  resolver_ptr->set_fail_siblings_on_cancel(true);
  socket_manager_remote_.reset();
  ASSERT_TRUE(base::test::RunUntil([&] { return !socket_manager_; }));
  EXPECT_EQ(resolver_ptr->pending_count(), 0u);
}

// Verifies ~P2PSocketManager() safely handles synchronous sibling DNS failures
// triggered while tearing down `dns_requests_`.
TEST_F(P2PSocketManagerTest, DestructorHandlesReentrantDnsFailure) {
  auto resolver = std::make_unique<ReentrantFailingHostResolver>();
  ReentrantFailingHostResolver* resolver_ptr = resolver.get();
  SetUpSocketManager(std::move(resolver));

  constexpr size_t kNumRequests = 4;
  for (size_t i = 0; i < kNumRequests; ++i) {
    socket_manager_remote_->GetHostAddress(
        "stun" + base::NumberToString(i) + ".example",
        /*address_family=*/std::nullopt, /*enable_mdns=*/false,
        base::DoNothing());
  }
  resolver_ptr->WaitForPendingRequests(kNumRequests);

  resolver_ptr->set_fail_siblings_on_cancel(true);
  socket_manager_remote_.reset();
  ASSERT_TRUE(base::test::RunUntil([&] { return !socket_manager_; }));
  EXPECT_EQ(resolver_ptr->pending_count(), 0u);
}

}  // namespace network
