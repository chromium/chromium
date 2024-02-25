// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolver_factory_mojo.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/proxy_resolver_error_observer.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test::IsError;
using net::test::IsOk;

namespace network {

namespace {

const char kScriptData[] = "FooBarBaz";
const char kExampleUrl[] = "http://www.example.com";

struct CreateProxyResolverAction {
  enum Action {
    COMPLETE,
    DROP_CLIENT,
    DROP_RESOLVER,
    DROP_BOTH,
    WAIT_FOR_CLIENT_DISCONNECT,
    MAKE_DNS_REQUEST,
  };

  static CreateProxyResolverAction ReturnResult(
      const std::string& expected_pac_script,
      net::Error error) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.error = error;
    return result;
  }

  static CreateProxyResolverAction DropClient(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = DROP_CLIENT;
    return result;
  }

  static CreateProxyResolverAction DropResolver(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = DROP_RESOLVER;
    return result;
  }

  static CreateProxyResolverAction DropBoth(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = DROP_BOTH;
    return result;
  }

  static CreateProxyResolverAction WaitForClientDisconnect(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = WAIT_FOR_CLIENT_DISCONNECT;
    return result;
  }

  static CreateProxyResolverAction MakeDnsRequest(
      const std::string& expected_pac_script) {
    CreateProxyResolverAction result;
    result.expected_pac_script = expected_pac_script;
    result.action = MAKE_DNS_REQUEST;
    return result;
  }

  std::string expected_pac_script;
  Action action = COMPLETE;
  net::Error error = net::OK;
};

struct GetProxyForUrlAction {
  enum Action {
    COMPLETE,
    // Drop the request by closing the reply channel.
    DROP,
    // Disconnect the service.
    DISCONNECT,
    // Wait for the client pipe to be disconnected.
    WAIT_FOR_CLIENT_DISCONNECT,
    // Make a DNS request.
    MAKE_DNS_REQUEST,
  };

  GetProxyForUrlAction() {}
  GetProxyForUrlAction(const GetProxyForUrlAction& other) = default;

  static GetProxyForUrlAction ReturnError(const GURL& url, net::Error error) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.error = error;
    return result;
  }

  static GetProxyForUrlAction ReturnServers(const GURL& url,
                                            const net::ProxyInfo& proxy_info) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.proxy_info = proxy_info;
    return result;
  }

  static GetProxyForUrlAction DropRequest(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = DROP;
    return result;
  }

  static GetProxyForUrlAction Disconnect(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = DISCONNECT;
    return result;
  }

  static GetProxyForUrlAction WaitForClientDisconnect(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = WAIT_FOR_CLIENT_DISCONNECT;
    return result;
  }

  static GetProxyForUrlAction MakeDnsRequest(const GURL& url) {
    GetProxyForUrlAction result;
    result.expected_url = url;
    result.action = MAKE_DNS_REQUEST;
    return result;
  }

  Action action = COMPLETE;
  net::Error error = net::OK;
  net::ProxyInfo proxy_info;
  GURL expected_url;
};

class MockMojoProxyResolver : public proxy_resolver::mojom::ProxyResolver {
 public:
  MockMojoProxyResolver();
  ~MockMojoProxyResolver() override;

  void AddGetProxyAction(GetProxyForUrlAction action);

  void WaitForNextRequest();

  void ClearBlockedClients();

  void AddConnection(
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver);

 private:
  // Overridden from proxy_resolver::mojom::ProxyResolver:
  void GetProxyForUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverRequestClient>
          pending_client) override;

  void WakeWaiter();

  std::string pac_script_data_;

  base::queue<GetProxyForUrlAction> get_proxy_actions_;

  base::OnceClosure quit_closure_;

  std::vector<mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient>>
      blocked_clients_;
  mojo::Receiver<proxy_resolver::mojom::ProxyResolver> receiver_{this};
};

MockMojoProxyResolver::~MockMojoProxyResolver() {
  EXPECT_TRUE(get_proxy_actions_.empty())
      << "Actions remaining: " << get_proxy_actions_.size();
}

MockMojoProxyResolver::MockMojoProxyResolver() = default;

void MockMojoProxyResolver::AddGetProxyAction(GetProxyForUrlAction action) {
  get_proxy_actions_.push(action);
}

void MockMojoProxyResolver::WaitForNextRequest() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockMojoProxyResolver::WakeWaiter() {
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

void MockMojoProxyResolver::ClearBlockedClients() {
  blocked_clients_.clear();
}

void MockMojoProxyResolver::AddConnection(
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void MockMojoProxyResolver::GetProxyForUrl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverRequestClient>
        pending_client) {
  ASSERT_FALSE(get_proxy_actions_.empty());
  GetProxyForUrlAction action = get_proxy_actions_.front();
  get_proxy_actions_.pop();

  EXPECT_EQ(action.expected_url, url);
  mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient> client(
      std::move(pending_client));
  client->Alert(url.spec());
  client->OnError(12345, url.spec());
  switch (action.action) {
    case GetProxyForUrlAction::COMPLETE: {
      client->ReportResult(action.error, action.proxy_info);
      break;
    }
    case GetProxyForUrlAction::DROP: {
      client.reset();
      break;
    }
    case GetProxyForUrlAction::DISCONNECT: {
      receiver_.reset();
      break;
    }
    case GetProxyForUrlAction::WAIT_FOR_CLIENT_DISCONNECT: {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      client.set_disconnect_handler(run_loop.QuitClosure());
      run_loop.Run();
      ASSERT_FALSE(client.is_connected());
      break;
    }
    case GetProxyForUrlAction::MAKE_DNS_REQUEST: {
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          dns_client;
      std::ignore = dns_client.InitWithNewPipeAndPassReceiver();
      client->ResolveDns(url.host(),
                         net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
                         network_anonymization_key, std::move(dns_client));
      blocked_clients_.push_back(std::move(client));
      break;
    }
  }
  WakeWaiter();
}

class Request {
 public:
  Request(net::ProxyResolver* resolver,
          const GURL& url,
          const net::NetworkAnonymizationKey& network_anonymization_key);

  int Resolve();
  void Cancel();
  int WaitForResult();

  const net::ProxyInfo& results() const { return results_; }
  net::LoadState load_state() { return request_->GetLoadState(); }
  net::NetLogWithSource& net_log_with_source() { return net_log_with_source_; }
  const net::TestCompletionCallback& callback() const { return callback_; }

 private:
  raw_ptr<net::ProxyResolver> resolver_;
  const GURL url_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
  net::ProxyInfo results_;
  std::unique_ptr<net::ProxyResolver::Request> request_;
  int error_;
  net::TestCompletionCallback callback_;
  net::NetLogWithSource net_log_with_source_{
      net::NetLogWithSource::Make(net::NetLogSourceType::NONE)};
};

Request::Request(net::ProxyResolver* resolver,
                 const GURL& url,
                 const net::NetworkAnonymizationKey& network_anonymization_key)
    : resolver_(resolver),
      url_(url),
      network_anonymization_key_(network_anonymization_key),
      error_(0) {}

int Request::Resolve() {
  error_ = resolver_->GetProxyForURL(url_, network_anonymization_key_,
                                     &results_, callback_.callback(), &request_,
                                     net_log_with_source_);
  return error_;
}

void Request::Cancel() {
  request_.reset();
}

int Request::WaitForResult() {
  error_ = callback_.WaitForResult();
  return error_;
}

class MockMojoProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory {
 public:
  MockMojoProxyResolverFactory(
      MockMojoProxyResolver* resolver,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolverFactory>
          receiver);
  ~MockMojoProxyResolverFactory() override;

  void AddCreateProxyResolverAction(CreateProxyResolverAction action);

  void WaitForNextRequest();

  void ClearBlockedClients();

  void RespondToBlockedClientsWithResult(net::Error error);

 private:
  // Overridden from proxy_resolver::mojom::ProxyResolver:
  void CreateResolver(
      const std::string& pac_url,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
          pending_client) override;

  void WakeWaiter();

  raw_ptr<MockMojoProxyResolver> resolver_;
  base::queue<CreateProxyResolverAction> create_resolver_actions_;

  base::OnceClosure quit_closure_;

  std::vector<
      mojo::Remote<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>>
      blocked_clients_;
  std::vector<mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver>>
      blocked_resolver_receivers_;
  mojo::Receiver<proxy_resolver::mojom::ProxyResolverFactory> receiver_;
};

MockMojoProxyResolverFactory::MockMojoProxyResolverFactory(
    MockMojoProxyResolver* resolver,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolverFactory> receiver)
    : resolver_(resolver), receiver_(this, std::move(receiver)) {}

MockMojoProxyResolverFactory::~MockMojoProxyResolverFactory() {
  EXPECT_TRUE(create_resolver_actions_.empty())
      << "Actions remaining: " << create_resolver_actions_.size();
}

void MockMojoProxyResolverFactory::AddCreateProxyResolverAction(
    CreateProxyResolverAction action) {
  create_resolver_actions_.push(action);
}

void MockMojoProxyResolverFactory::WaitForNextRequest() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockMojoProxyResolverFactory::WakeWaiter() {
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

void MockMojoProxyResolverFactory::ClearBlockedClients() {
  blocked_clients_.clear();
}

void MockMojoProxyResolverFactory::RespondToBlockedClientsWithResult(
    net::Error error) {
  for (const auto& client : blocked_clients_) {
    client->ReportResult(error);
  }
}

void MockMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
        pending_client) {
  ASSERT_FALSE(create_resolver_actions_.empty());
  CreateProxyResolverAction action = create_resolver_actions_.front();
  create_resolver_actions_.pop();

  EXPECT_EQ(action.expected_pac_script, pac_script);
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client(
      std::move(pending_client));
  client->Alert(pac_script);
  client->OnError(12345, pac_script);
  switch (action.action) {
    case CreateProxyResolverAction::COMPLETE: {
      if (action.error == net::OK)
        resolver_->AddConnection(std::move(receiver));
      client->ReportResult(action.error);
      break;
    }
    case CreateProxyResolverAction::DROP_CLIENT: {
      // Save |receiver| so its pipe isn't closed.
      blocked_resolver_receivers_.push_back(std::move(receiver));
      break;
    }
    case CreateProxyResolverAction::DROP_RESOLVER: {
      // Save |client| so its pipe isn't closed.
      blocked_clients_.push_back(std::move(client));
      break;
    }
    case CreateProxyResolverAction::DROP_BOTH: {
      // Both |receiver| and |client| will be closed.
      break;
    }
    case CreateProxyResolverAction::WAIT_FOR_CLIENT_DISCONNECT: {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      client.set_disconnect_handler(run_loop.QuitClosure());
      run_loop.Run();
      ASSERT_FALSE(client.is_connected());
      break;
    }
    case CreateProxyResolverAction::MAKE_DNS_REQUEST: {
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          dns_client;
      std::ignore = dns_client.InitWithNewPipeAndPassReceiver();
      client->ResolveDns(pac_script,
                         net::ProxyResolveDnsOperation::DNS_RESOLVE_EX,
                         net::NetworkAnonymizationKey(), std::move(dns_client));
      blocked_clients_.push_back(std::move(client));
      break;
    }
  }
  WakeWaiter();
}

void DeleteResolverFactoryRequestCallback(
    std::unique_ptr<net::ProxyResolverFactory::Request>* request,
    net::CompletionOnceCallback callback,
    int result) {
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->get());
  request->reset();
  std::move(callback).Run(result);
}

void CheckCapturedNetLogEntries(const std::string& expected_string,
                                const std::vector<net::NetLogEntry>& entries) {
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(net::NetLogEventType::PAC_JAVASCRIPT_ALERT, entries[0].type);
  EXPECT_EQ(expected_string,
            net::GetStringValueFromParams(entries[0], "message"));
  ASSERT_FALSE(entries[0].params.contains("line_number"));
  EXPECT_EQ(net::NetLogEventType::PAC_JAVASCRIPT_ERROR, entries[1].type);
  EXPECT_EQ(expected_string,
            net::GetStringValueFromParams(entries[1], "message"));
  EXPECT_EQ(12345, net::GetIntegerValueFromParams(entries[1], "line_number"));
}

}  // namespace

class ProxyResolverFactoryMojoTest : public testing::Test {
 public:
  void SetUp() override {
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
        factory_remote;
    mock_proxy_resolver_factory_ =
        std::make_unique<MockMojoProxyResolverFactory>(
            &mock_proxy_resolver_,
            factory_remote.InitWithNewPipeAndPassReceiver());
    proxy_resolver_factory_mojo_ = std::make_unique<ProxyResolverFactoryMojo>(
        std::move(factory_remote), &host_resolver_, base::NullCallback(),
        net::NetLog::Get());
  }

  std::unique_ptr<Request> MakeRequest(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key =
          net::NetworkAnonymizationKey()) {
    return std::make_unique<Request>(proxy_resolver_mojo_.get(), url,
                                     network_anonymization_key);
  }

  net::ProxyInfo ProxyServersFromPacString(const std::string& pac_string) {
    net::ProxyInfo proxy_info;
    proxy_info.UsePacString(pac_string);
    return proxy_info;
  }

  void CreateProxyResolver() {
    mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
        CreateProxyResolverAction::ReturnResult(kScriptData, net::OK));
    net::TestCompletionCallback callback;
    scoped_refptr<net::PacFileData> pac_script(
        net::PacFileData::FromUTF8(kScriptData));
    std::unique_ptr<net::ProxyResolverFactory::Request> request;
    ASSERT_EQ(
        net::OK,
        callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
            pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
    EXPECT_TRUE(request);
    ASSERT_TRUE(proxy_resolver_mojo_);
  }

  void DeleteProxyResolverCallback(net::CompletionOnceCallback callback,
                                   int result) {
    proxy_resolver_mojo_.reset();
    std::move(callback).Run(result);
  }

  base::test::TaskEnvironment task_environment_;
  net::HangingHostResolver host_resolver_;
  net::RecordingNetLogObserver net_log_observer_;
  std::unique_ptr<MockMojoProxyResolverFactory> mock_proxy_resolver_factory_;
  std::unique_ptr<net::ProxyResolverFactory> proxy_resolver_factory_mojo_;

  MockMojoProxyResolver mock_proxy_resolver_;
  std::unique_ptr<net::ProxyResolver> proxy_resolver_mojo_;
};

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver) {
  CreateProxyResolver();
  CheckCapturedNetLogEntries(kScriptData, net_log_observer_.GetEntries());
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_Empty) {
  net::TestCompletionCallback callback;
  scoped_refptr<net::PacFileData> pac_script(net::PacFileData::FromUTF8(""));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  EXPECT_EQ(
      net::ERR_PAC_SCRIPT_FAILED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_FALSE(request);
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_Url) {
  net::TestCompletionCallback callback;
  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromURL(GURL(kExampleUrl)));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  EXPECT_EQ(
      net::ERR_PAC_SCRIPT_FAILED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_FALSE(request);
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_Failed) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::ReturnResult(
          kScriptData, net::ERR_HTTP_RESPONSE_CODE_FAILURE));

  net::TestCompletionCallback callback;
  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  EXPECT_EQ(
      net::ERR_HTTP_RESPONSE_CODE_FAILURE,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);

  // A second attempt succeeds.
  CreateProxyResolver();
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_BothDisconnected) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropBoth(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  net::TestCompletionCallback callback;
  EXPECT_EQ(
      net::ERR_PAC_SCRIPT_TERMINATED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_ClientDisconnected) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropClient(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  net::TestCompletionCallback callback;
  EXPECT_EQ(
      net::ERR_PAC_SCRIPT_TERMINATED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request)));
  EXPECT_TRUE(request);
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_ResolverDisconnected) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropResolver(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;

  // When the ResolverRequest pipe is dropped, the ProxyResolverFactory should
  // still wait to get an error from the client pipe.
  net::TestCompletionCallback callback;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request));
  EXPECT_TRUE(request);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());

  mock_proxy_resolver_factory_->RespondToBlockedClientsWithResult(
      net::ERR_PAC_SCRIPT_FAILED);
  EXPECT_EQ(net::ERR_PAC_SCRIPT_FAILED, callback.WaitForResult());
}

// The resolver pipe is dropped, but the client is told the request succeeded
// (This could happen if a proxy resolver is created successfully, but then the
// proxy crashes before the client reads the success message).
TEST_F(ProxyResolverFactoryMojoTest,
       CreateProxyResolver_ResolverDisconnectedButClientSucceeded) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropResolver(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;

  // When the ResolverRequest pipe is dropped, the ProxyResolverFactory
  // shouldn't notice, and should just continue to wait for a response on the
  // other pipe.
  net::TestCompletionCallback create_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            proxy_resolver_factory_mojo_->CreateProxyResolver(
                pac_script, &proxy_resolver_mojo_, create_callback.callback(),
                &request));
  EXPECT_TRUE(request);
  mock_proxy_resolver_factory_->WaitForNextRequest();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(create_callback.have_result());

  // The client pipe reports success!
  mock_proxy_resolver_factory_->RespondToBlockedClientsWithResult(net::OK);
  EXPECT_EQ(net::OK, create_callback.WaitForResult());

  // Proxy resolutions should fail with ERR_PAC_SCRIPT_TERMINATED, however. That
  // error should normally cause the ProxyResolutionService to destroy the
  // resolver.
  net::ProxyInfo results;
  std::unique_ptr<net::ProxyResolver::Request> pac_request;
  net::TestCompletionCallback delete_callback;
  EXPECT_EQ(net::ERR_PAC_SCRIPT_TERMINATED,
            delete_callback.GetResult(proxy_resolver_mojo_->GetProxyForURL(
                GURL(kExampleUrl), net::NetworkAnonymizationKey(), &results,
                base::BindOnce(
                    &ProxyResolverFactoryMojoTest::DeleteProxyResolverCallback,
                    base::Unretained(this), delete_callback.callback()),
                &pac_request, net::NetLogWithSource())));
}

TEST_F(ProxyResolverFactoryMojoTest,
       CreateProxyResolver_ResolverDisconnected_DeleteRequestInCallback) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::DropClient(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  net::TestCompletionCallback callback;
  EXPECT_EQ(
      net::ERR_PAC_SCRIPT_TERMINATED,
      callback.GetResult(proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_,
          base::BindOnce(&DeleteResolverFactoryRequestCallback, &request,
                         callback.callback()),
          &request)));
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_Cancel) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::WaitForClientDisconnect(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  net::TestCompletionCallback callback;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request));
  ASSERT_TRUE(request);
  request.reset();

  // The Mojo request is still made.
  mock_proxy_resolver_factory_->WaitForNextRequest();
}

TEST_F(ProxyResolverFactoryMojoTest, CreateProxyResolver_DnsRequest) {
  mock_proxy_resolver_factory_->AddCreateProxyResolverAction(
      CreateProxyResolverAction::MakeDnsRequest(kScriptData));

  scoped_refptr<net::PacFileData> pac_script(
      net::PacFileData::FromUTF8(kScriptData));
  std::unique_ptr<net::ProxyResolverFactory::Request> request;
  net::TestCompletionCallback callback;
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      proxy_resolver_factory_mojo_->CreateProxyResolver(
          pac_script, &proxy_resolver_mojo_, callback.callback(), &request));
  ASSERT_TRUE(request);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  mock_proxy_resolver_factory_->ClearBlockedClients();
  callback.WaitForResult();
  EXPECT_EQ(1, host_resolver_.num_cancellations());
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL) {
  const GURL url(kExampleUrl);
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      url, ProxyServersFromPacString("DIRECT")));
  CreateProxyResolver();
  net_log_observer_.Clear();

  std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(request->WaitForResult(), IsOk());

  EXPECT_EQ("DIRECT", request->results().ToDebugString());
  CheckCapturedNetLogEntries(url.spec(),
                             net_log_observer_.GetEntriesForSource(
                                 request->net_log_with_source().source()));
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_MultipleResults) {
  static const char kPacString[] =
      "PROXY foo1:80;DIRECT;SOCKS foo2:1234;"
      "SOCKS5 foo3:1080;HTTPS foo4:443";
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString(kPacString)));
  CreateProxyResolver();

  std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(request->WaitForResult(), IsOk());

  EXPECT_EQ(kPacString, request->results().ToDebugString());
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_Error) {
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnError(
      GURL(kExampleUrl), net::ERR_UNEXPECTED));
  CreateProxyResolver();

  std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(request->WaitForResult(), IsError(net::ERR_UNEXPECTED));

  EXPECT_TRUE(request->results().is_empty());
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_Cancel) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::WaitForClientDisconnect(GURL(kExampleUrl)));
  CreateProxyResolver();

  std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
  request->Cancel();
  EXPECT_FALSE(request->callback().have_result());

  // The Mojo request is still made.
  mock_proxy_resolver_.WaitForNextRequest();
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_MultipleRequests) {
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL("https://www.chromium.org"),
      ProxyServersFromPacString("HTTPS foo:443")));
  CreateProxyResolver();

  std::unique_ptr<Request> request1(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request1->Resolve(), IsError(net::ERR_IO_PENDING));
  std::unique_ptr<Request> request2(
      MakeRequest(GURL("https://www.chromium.org")));
  EXPECT_THAT(request2->Resolve(), IsError(net::ERR_IO_PENDING));

  EXPECT_THAT(request1->WaitForResult(), IsOk());
  EXPECT_THAT(request2->WaitForResult(), IsOk());

  EXPECT_EQ("DIRECT", request1->results().ToDebugString());
  EXPECT_EQ("HTTPS foo:443", request2->results().ToDebugString());
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_Disconnect) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));
  CreateProxyResolver();
  {
    std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
    EXPECT_THAT(request->WaitForResult(),
                IsError(net::ERR_PAC_SCRIPT_TERMINATED));
    EXPECT_TRUE(request->results().is_empty());
  }

  // Run Watcher::OnHandleReady() tasks posted by Watcher::CallOnHandleReady().
  base::RunLoop().RunUntilIdle();

  {
    // Calling GetProxyForURL after a disconnect should fail.
    std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
    EXPECT_THAT(request->Resolve(), IsError(net::ERR_PAC_SCRIPT_TERMINATED));
  }
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_ClientClosed) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::DropRequest(GURL(kExampleUrl)));
  CreateProxyResolver();

  std::unique_ptr<Request> request1(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request1->Resolve(), IsError(net::ERR_IO_PENDING));

  EXPECT_THAT(request1->WaitForResult(),
              IsError(net::ERR_PAC_SCRIPT_TERMINATED));
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_DeleteInCallback) {
  mock_proxy_resolver_.AddGetProxyAction(GetProxyForUrlAction::ReturnServers(
      GURL(kExampleUrl), ProxyServersFromPacString("DIRECT")));
  CreateProxyResolver();

  net::ProxyInfo results;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolver::Request> request;
  net::NetLogWithSource net_log;
  EXPECT_EQ(net::OK,
            callback.GetResult(proxy_resolver_mojo_->GetProxyForURL(
                GURL(kExampleUrl), net::NetworkAnonymizationKey(), &results,
                base::BindOnce(
                    &ProxyResolverFactoryMojoTest::DeleteProxyResolverCallback,
                    base::Unretained(this), callback.callback()),
                &request, net_log)));
}

TEST_F(ProxyResolverFactoryMojoTest,
       GetProxyForURL_DeleteInCallbackFromDisconnect) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::Disconnect(GURL(kExampleUrl)));
  CreateProxyResolver();

  net::ProxyInfo results;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::ProxyResolver::Request> request;
  net::NetLogWithSource net_log;
  EXPECT_EQ(net::ERR_PAC_SCRIPT_TERMINATED,
            callback.GetResult(proxy_resolver_mojo_->GetProxyForURL(
                GURL(kExampleUrl), net::NetworkAnonymizationKey(), &results,
                base::BindOnce(
                    &ProxyResolverFactoryMojoTest::DeleteProxyResolverCallback,
                    base::Unretained(this), callback.callback()),
                &request, net_log)));
}

TEST_F(ProxyResolverFactoryMojoTest, GetProxyForURL_DnsRequest) {
  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::MakeDnsRequest(GURL(kExampleUrl)));
  CreateProxyResolver();

  std::unique_ptr<Request> request(MakeRequest(GURL(kExampleUrl)));
  EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
  EXPECT_EQ(net::LOAD_STATE_RESOLVING_PROXY_FOR_URL, request->load_state());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  mock_proxy_resolver_.ClearBlockedClients();
  request->WaitForResult();
  EXPECT_EQ(1, host_resolver_.num_cancellations());
}

TEST_F(ProxyResolverFactoryMojoTest,
       GetProxyForURL_DnsRequestWithNetworkAnonymizationKey) {
  net::SchemefulSite kSite =
      net::SchemefulSite(url::Origin::Create(GURL("https://origin.test/")));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);
  const GURL kUrl(kExampleUrl);

  mock_proxy_resolver_.AddGetProxyAction(
      GetProxyForUrlAction::MakeDnsRequest(kUrl));
  CreateProxyResolver();

  std::unique_ptr<Request> request(MakeRequest(kUrl, kNetworkAnonymizationKey));
  EXPECT_THAT(request->Resolve(), IsError(net::ERR_IO_PENDING));
  EXPECT_EQ(net::LOAD_STATE_RESOLVING_PROXY_FOR_URL, request->load_state());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(kUrl.host(), host_resolver_.last_host().host());
  EXPECT_EQ(kNetworkAnonymizationKey,
            host_resolver_.last_network_anonymization_key());
}

TEST_F(ProxyResolverFactoryMojoTest, DeleteResolver) {
  CreateProxyResolver();
  proxy_resolver_mojo_.reset();
}
}  // namespace network
