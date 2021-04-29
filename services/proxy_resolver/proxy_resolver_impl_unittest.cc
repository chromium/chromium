// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test::IsError;
using net::test::IsOk;

namespace proxy_resolver {
namespace {

class TestRequestClient : public mojom::ProxyResolverRequestClient {
 public:
  enum Event {
    RESULT_RECEIVED,
    CONNECTION_ERROR,
  };

  explicit TestRequestClient(
      mojo::PendingReceiver<mojom::ProxyResolverRequestClient> receiver);

  void WaitForResult();

  net::Error error() { return error_; }
  const net::ProxyInfo& results() { return results_; }
  net::EventWaiter<Event>& event_waiter() { return event_waiter_; }

 private:
  // mojom::ProxyResolverRequestClient override.
  void ReportResult(int32_t error, const net::ProxyInfo& results) override;
  void Alert(const std::string& message) override;
  void OnError(int32_t line_number, const std::string& message) override;
  void ResolveDns(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<mojom::HostResolverRequestClient> client) override;

  void OnDisconnect();

  bool done_ = false;
  net::Error error_ = net::ERR_FAILED;
  net::ProxyInfo results_;

  mojo::Receiver<mojom::ProxyResolverRequestClient> receiver_;

  net::EventWaiter<Event> event_waiter_;
};

TestRequestClient::TestRequestClient(
    mojo::PendingReceiver<mojom::ProxyResolverRequestClient> receiver)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&TestRequestClient::OnDisconnect, base::Unretained(this)));
}

void TestRequestClient::WaitForResult() {
  if (done_)
    return;

  event_waiter_.WaitForEvent(RESULT_RECEIVED);
  ASSERT_TRUE(done_);
}

void TestRequestClient::ReportResult(int32_t error,
                                     const net::ProxyInfo& results) {
  event_waiter_.NotifyEvent(RESULT_RECEIVED);
  ASSERT_FALSE(done_);
  error_ = static_cast<net::Error>(error);
  results_ = results;
  done_ = true;
}

void TestRequestClient::Alert(const std::string& message) {}

void TestRequestClient::OnError(int32_t line_number,
                                const std::string& message) {}

void TestRequestClient::ResolveDns(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    const net::NetworkIsolationKey& network_isolation_key,
    mojo::PendingRemote<mojom::HostResolverRequestClient> client) {}

void TestRequestClient::OnDisconnect() {
  event_waiter_.NotifyEvent(CONNECTION_ERROR);
}

class MockProxyResolverV8Tracing : public ProxyResolverV8Tracing {
 public:
  // TODO(mmenke): This struct violates the Google style guide, as structs
  // aren't allowed to have private members. Fix that.
  struct Job {
    GURL url;
    net::NetworkIsolationKey network_isolation_key;
    net::ProxyInfo* results;
    bool cancelled = false;

    void Complete(int result) {
      DCHECK(!callback_.is_null());
      std::move(callback_).Run(result);
    }

    bool WasCompleted() { return callback_.is_null(); }

    void SetCallback(net::CompletionOnceCallback callback) {
      callback_ = std::move(callback);
    }

   private:
    net::CompletionOnceCallback callback_;
  };

  class RequestImpl : public net::ProxyResolver::Request {
   public:
    RequestImpl(Job* job, MockProxyResolverV8Tracing* resolver)
        : job_(job), resolver_(resolver) {}

    ~RequestImpl() override {
      if (job_->WasCompleted())
        return;
      job_->cancelled = true;
      if (resolver_->cancel_callback_)
        std::move(resolver_->cancel_callback_).Run();
    }

    net::LoadState GetLoadState() override {
      return net::LOAD_STATE_RESOLVING_PROXY_FOR_URL;
    }

   private:
    Job* job_;
    MockProxyResolverV8Tracing* resolver_;
  };

  MockProxyResolverV8Tracing() {}

  // ProxyResolverV8Tracing overrides.
  void GetProxyForURL(const GURL& url,
                      const net::NetworkIsolationKey& network_isolation_key,
                      net::ProxyInfo* results,
                      net::CompletionOnceCallback callback,
                      std::unique_ptr<net::ProxyResolver::Request>* request,
                      std::unique_ptr<Bindings> bindings) override;

  void WaitForCancel();

  const std::vector<std::unique_ptr<Job>>& pending_jobs() {
    return pending_jobs_;
  }

 private:
  base::OnceClosure cancel_callback_;
  std::vector<std::unique_ptr<Job>> pending_jobs_;
};

void MockProxyResolverV8Tracing::GetProxyForURL(
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    net::ProxyInfo* results,
    net::CompletionOnceCallback callback,
    std::unique_ptr<net::ProxyResolver::Request>* request,
    std::unique_ptr<Bindings> bindings) {
  pending_jobs_.push_back(std::make_unique<Job>());
  auto* pending_job = pending_jobs_.back().get();
  pending_job->url = url;
  pending_job->network_isolation_key = network_isolation_key;
  pending_job->results = results;
  pending_job->SetCallback(std::move(callback));
  *request = std::make_unique<RequestImpl>(pending_job, this);
}

void MockProxyResolverV8Tracing::WaitForCancel() {
  while (std::find_if(pending_jobs_.begin(), pending_jobs_.end(),
                      [](const std::unique_ptr<Job>& job) {
                        return job->cancelled;
                      }) != pending_jobs_.end()) {
    base::RunLoop run_loop;
    cancel_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

}  // namespace

class ProxyResolverImplTest : public testing::Test {
 public:
  ProxyResolverImplTest() {
    std::unique_ptr<MockProxyResolverV8Tracing> mock_resolver =
        std::make_unique<MockProxyResolverV8Tracing>();
    mock_proxy_resolver_ = mock_resolver.get();
    resolver_impl_ =
        std::make_unique<ProxyResolverImpl>(std::move(mock_resolver));
    resolver_ = resolver_impl_.get();
  }

  ~ProxyResolverImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MockProxyResolverV8Tracing* mock_proxy_resolver_;

  std::unique_ptr<ProxyResolverImpl> resolver_impl_;
  mojom::ProxyResolver* resolver_;
};

TEST_F(ProxyResolverImplTest, GetProxyForUrl) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkIsolationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);

  job->results->UsePacString(
      "PROXY proxy.example.com:1; "
      "SOCKS4 socks4.example.com:2; "
      "SOCKS5 socks5.example.com:3; "
      "HTTPS https.example.com:4; "
      "QUIC quic.example.com:65000; "
      "DIRECT");
  job->Complete(net::OK);
  client.WaitForResult();

  EXPECT_THAT(client.error(), IsOk());
  std::vector<net::ProxyServer> servers =
      client.results().proxy_list().GetAll();
  ASSERT_EQ(6u, servers.size());
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTP, servers[0].scheme());
  EXPECT_EQ("proxy.example.com", servers[0].host_port_pair().host());
  EXPECT_EQ(1, servers[0].host_port_pair().port());

  EXPECT_EQ(net::ProxyServer::SCHEME_SOCKS4, servers[1].scheme());
  EXPECT_EQ("socks4.example.com", servers[1].host_port_pair().host());
  EXPECT_EQ(2, servers[1].host_port_pair().port());

  EXPECT_EQ(net::ProxyServer::SCHEME_SOCKS5, servers[2].scheme());
  EXPECT_EQ("socks5.example.com", servers[2].host_port_pair().host());
  EXPECT_EQ(3, servers[2].host_port_pair().port());

  EXPECT_EQ(net::ProxyServer::SCHEME_HTTPS, servers[3].scheme());
  EXPECT_EQ("https.example.com", servers[3].host_port_pair().host());
  EXPECT_EQ(4, servers[3].host_port_pair().port());

  EXPECT_EQ(net::ProxyServer::SCHEME_QUIC, servers[4].scheme());
  EXPECT_EQ("quic.example.com", servers[4].host_port_pair().host());
  EXPECT_EQ(65000, servers[4].host_port_pair().port());

  EXPECT_EQ(net::ProxyServer::SCHEME_DIRECT, servers[5].scheme());
}

TEST_F(ProxyResolverImplTest, GetProxyForUrlWithNetworkIsolationKey) {
  const url::Origin kOrigin(url::Origin::Create(GURL("https://origin.test/")));
  const net::NetworkIsolationKey kNetworkIsolationKey(kOrigin, kOrigin);

  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"), kNetworkIsolationKey,
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);
  EXPECT_EQ(kNetworkIsolationKey, job->network_isolation_key);
}

TEST_F(ProxyResolverImplTest, GetProxyForUrlFailure) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkIsolationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);
  job->Complete(net::ERR_FAILED);
  client.WaitForResult();

  EXPECT_THAT(client.error(), IsError(net::ERR_FAILED));
  std::vector<net::ProxyServer> proxy_servers =
      client.results().proxy_list().GetAll();
  EXPECT_TRUE(proxy_servers.empty());
}

TEST_F(ProxyResolverImplTest, GetProxyForUrlMultiple) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client1;
  TestRequestClient client1(remote_client1.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client2;
  TestRequestClient client2(remote_client2.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkIsolationKey(),
                            std::move(remote_client1));
  resolver_->GetProxyForUrl(GURL("https://example.com"),
                            net::NetworkIsolationKey(),
                            std::move(remote_client2));
  ASSERT_EQ(2u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job1 =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job1->url);
  MockProxyResolverV8Tracing::Job* job2 =
      mock_proxy_resolver_->pending_jobs()[1].get();
  EXPECT_EQ(GURL("https://example.com"), job2->url);
  job1->results->UsePacString("HTTPS proxy.example.com:12345");
  job1->Complete(net::OK);
  job2->results->UsePacString("SOCKS5 another-proxy.example.com:6789");
  job2->Complete(net::OK);
  client1.WaitForResult();
  client2.WaitForResult();

  EXPECT_THAT(client1.error(), IsOk());
  std::vector<net::ProxyServer> proxy_servers1 =
      client1.results().proxy_list().GetAll();
  ASSERT_EQ(1u, proxy_servers1.size());
  net::ProxyServer& server1 = proxy_servers1[0];
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTPS, server1.scheme());
  EXPECT_EQ("proxy.example.com", server1.host_port_pair().host());
  EXPECT_EQ(12345, server1.host_port_pair().port());

  EXPECT_THAT(client2.error(), IsOk());
  std::vector<net::ProxyServer> proxy_servers2 =
      client2.results().proxy_list().GetAll();
  ASSERT_EQ(1u, proxy_servers1.size());
  net::ProxyServer& server2 = proxy_servers2[0];
  EXPECT_EQ(net::ProxyServer::SCHEME_SOCKS5, server2.scheme());
  EXPECT_EQ("another-proxy.example.com", server2.host_port_pair().host());
  EXPECT_EQ(6789, server2.host_port_pair().port());
}

TEST_F(ProxyResolverImplTest, DestroyClient) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  auto client = std::make_unique<TestRequestClient>(
      remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkIsolationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  const MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);
  job->results->UsePacString("PROXY proxy.example.com:8080");
  client.reset();
  mock_proxy_resolver_->WaitForCancel();
}

TEST_F(ProxyResolverImplTest, DestroyService) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkIsolationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  resolver_impl_.reset();
  client.event_waiter().WaitForEvent(TestRequestClient::CONNECTION_ERROR);
}

}  // namespace proxy_resolver
