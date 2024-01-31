// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
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
      const net::NetworkAnonymizationKey& network_anonymization_key,
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
    const net::NetworkAnonymizationKey& network_anonymization_key,
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
    net::NetworkAnonymizationKey network_anonymization_key;
    raw_ptr<net::ProxyInfo, DanglingUntriaged> results;
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
    raw_ptr<Job> job_;
    raw_ptr<MockProxyResolverV8Tracing> resolver_;
  };

  MockProxyResolverV8Tracing() {}

  // ProxyResolverV8Tracing overrides.
  void GetProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
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
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::ProxyInfo* results,
    net::CompletionOnceCallback callback,
    std::unique_ptr<net::ProxyResolver::Request>* request,
    std::unique_ptr<Bindings> bindings) {
  pending_jobs_.push_back(std::make_unique<Job>());
  auto* pending_job = pending_jobs_.back().get();
  pending_job->url = url;
  pending_job->network_anonymization_key = network_anonymization_key;
  pending_job->results = results;
  pending_job->SetCallback(std::move(callback));
  *request = std::make_unique<RequestImpl>(pending_job, this);
}

void MockProxyResolverV8Tracing::WaitForCancel() {
  while (base::ranges::any_of(pending_jobs_, &Job::cancelled)) {
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
  raw_ptr<MockProxyResolverV8Tracing, DanglingUntriaged> mock_proxy_resolver_;

  std::unique_ptr<ProxyResolverImpl> resolver_impl_;
  raw_ptr<mojom::ProxyResolver, DanglingUntriaged> resolver_;
};

TEST_F(ProxyResolverImplTest, GetProxyForUrl) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkAnonymizationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);

  net::ProxyList proxy_list;
  proxy_list.AddProxyChain(net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, "proxy.example.com", 1));
  proxy_list.AddProxyChain(net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS4, "socks4.example.com", 2));
  proxy_list.AddProxyChain(net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS5, "socks5.example.com", 3));
  proxy_list.AddProxyChain(net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "https.example.com", 4));
  proxy_list.AddProxyChain(net::ProxyChain::Direct());

  job->results->UseProxyList(proxy_list);
  job->Complete(net::OK);
  client.WaitForResult();

  EXPECT_THAT(client.error(), IsOk());
  std::vector<net::ProxyChain> chains =
      client.results().proxy_list().AllChains();
  ASSERT_EQ(5u, chains.size());
  EXPECT_EQ("[proxy.example.com:1]", chains[0].ToDebugString());
  EXPECT_EQ("[socks4://socks4.example.com:2]", chains[1].ToDebugString());
  EXPECT_EQ("[socks5://socks5.example.com:3]", chains[2].ToDebugString());
  EXPECT_EQ("[https://https.example.com:4]", chains[3].ToDebugString());
  EXPECT_EQ(net::ProxyChain::Direct(), chains[4]);
}

TEST_F(ProxyResolverImplTest, GetProxyForUrlWithNetworkAnonymizationKey) {
  const net::SchemefulSite kSite(
      net::SchemefulSite(GURL("https://site.test/")));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);

  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            kNetworkAnonymizationKey, std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);
  EXPECT_EQ(kNetworkAnonymizationKey, job->network_anonymization_key);
}

TEST_F(ProxyResolverImplTest, GetProxyForUrlFailure) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  TestRequestClient client(remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkAnonymizationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  MockProxyResolverV8Tracing::Job* job =
      mock_proxy_resolver_->pending_jobs()[0].get();
  EXPECT_EQ(GURL("http://example.com"), job->url);
  job->Complete(net::ERR_FAILED);
  client.WaitForResult();

  EXPECT_THAT(client.error(), IsError(net::ERR_FAILED));
  std::vector<net::ProxyChain> proxy_chains =
      client.results().proxy_list().AllChains();
  EXPECT_TRUE(proxy_chains.empty());
}

TEST_F(ProxyResolverImplTest, GetProxyForUrlMultiple) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client1;
  TestRequestClient client1(remote_client1.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client2;
  TestRequestClient client2(remote_client2.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkAnonymizationKey(),
                            std::move(remote_client1));
  resolver_->GetProxyForUrl(GURL("https://example.com"),
                            net::NetworkAnonymizationKey(),
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
  std::vector<net::ProxyChain> proxy_chain1 =
      client1.results().proxy_list().AllChains();
  ASSERT_EQ(1u, proxy_chain1.size());
  const net::ProxyServer& server1 =
      proxy_chain1.at(0).GetProxyServer(/*chain_index=*/0);
  EXPECT_EQ(net::ProxyServer::SCHEME_HTTPS, server1.scheme());
  EXPECT_EQ("proxy.example.com", server1.host_port_pair().host());
  EXPECT_EQ(12345, server1.host_port_pair().port());

  EXPECT_THAT(client2.error(), IsOk());
  std::vector<net::ProxyChain> proxy_chain2 =
      client2.results().proxy_list().AllChains();
  ASSERT_EQ(1u, proxy_chain2.size());
  const net::ProxyServer& server2 =
      proxy_chain2.at(0).GetProxyServer(/*chain_index=*/0);
  EXPECT_EQ(net::ProxyServer::SCHEME_SOCKS5, server2.scheme());
  EXPECT_EQ("another-proxy.example.com", server2.host_port_pair().host());
  EXPECT_EQ(6789, server2.host_port_pair().port());
}

TEST_F(ProxyResolverImplTest, DestroyClient) {
  mojo::PendingRemote<mojom::ProxyResolverRequestClient> remote_client;
  auto client = std::make_unique<TestRequestClient>(
      remote_client.InitWithNewPipeAndPassReceiver());

  resolver_->GetProxyForUrl(GURL("http://example.com"),
                            net::NetworkAnonymizationKey(),
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
                            net::NetworkAnonymizationKey(),
                            std::move(remote_client));
  ASSERT_EQ(1u, mock_proxy_resolver_->pending_jobs().size());
  resolver_impl_.reset();
  client.event_waiter().WaitForEvent(TestRequestClient::CONNECTION_ERROR);
}

}  // namespace proxy_resolver
