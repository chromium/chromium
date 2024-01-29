// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_factory_impl.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace proxy_resolver {
namespace {

const char kScriptData[] = "FooBarBaz";
const char16_t kScriptData16[] = u"FooBarBaz";

class FakeProxyResolver : public ProxyResolverV8Tracing {
 public:
  explicit FakeProxyResolver(base::OnceClosure on_destruction)
      : on_destruction_(std::move(on_destruction)) {}

  ~FakeProxyResolver() override { std::move(on_destruction_).Run(); }

 private:
  // ProxyResolverV8Tracing overrides.
  void GetProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::ProxyInfo* results,
      net::CompletionOnceCallback callback,
      std::unique_ptr<net::ProxyResolver::Request>* request,
      std::unique_ptr<Bindings> bindings) override {}

  base::OnceClosure on_destruction_;
};

enum Event {
  NONE,
  RESOLVER_CREATED,
  CONNECTION_ERROR,
  RESOLVER_DESTROYED,
};

class TestProxyResolverFactory : public ProxyResolverV8TracingFactory {
 public:
  struct PendingRequest {
    raw_ptr<std::unique_ptr<ProxyResolverV8Tracing>,
            AcrossTasksDanglingUntriaged>
        resolver;
    net::CompletionOnceCallback callback;
  };

  explicit TestProxyResolverFactory(net::EventWaiter<Event>* waiter)
      : waiter_(waiter) {}

  ~TestProxyResolverFactory() override {}

  void CreateProxyResolverV8Tracing(
      const scoped_refptr<net::PacFileData>& pac_script,
      std::unique_ptr<ProxyResolverV8Tracing::Bindings> bindings,
      std::unique_ptr<ProxyResolverV8Tracing>* resolver,
      net::CompletionOnceCallback callback,
      std::unique_ptr<net::ProxyResolverFactory::Request>* request) override {
    requests_handled_++;
    waiter_->NotifyEvent(RESOLVER_CREATED);
    EXPECT_EQ(kScriptData16, pac_script->utf16());
    EXPECT_TRUE(resolver);
    pending_request_ = std::make_unique<PendingRequest>();
    pending_request_->resolver = resolver;
    pending_request_->callback = std::move(callback);

    ASSERT_TRUE(bindings);

    bindings->Alert(u"alert");
    bindings->OnError(10, u"error");
    EXPECT_TRUE(bindings->GetHostResolver());
  }

  size_t requests_handled() { return requests_handled_; }
  PendingRequest* pending_request() { return pending_request_.get(); }

 private:
  raw_ptr<net::EventWaiter<Event>> waiter_;
  size_t requests_handled_ = 0;
  std::unique_ptr<PendingRequest> pending_request_;
};

class TestProxyResolverFactoryImpl : public ProxyResolverFactoryImpl {
 public:
  TestProxyResolverFactoryImpl(
      mojo::PendingReceiver<mojom::ProxyResolverFactory> receiver,
      std::unique_ptr<ProxyResolverV8TracingFactory> factory)
      : ProxyResolverFactoryImpl(std::move(receiver), std::move(factory)) {}
};

}  // namespace

class ProxyResolverFactoryImplTest
    : public testing::Test,
      public mojom::ProxyResolverFactoryRequestClient {
 public:
  ProxyResolverFactoryImplTest() {
    std::unique_ptr<TestProxyResolverFactory> test_factory =
        std::make_unique<TestProxyResolverFactory>(&waiter_);
    mock_factory_ = test_factory.get();
    mock_factory_impl_ = std::make_unique<TestProxyResolverFactoryImpl>(
        factory_.BindNewPipeAndPassReceiver(), std::move(test_factory));
    factory_.set_idle_handler(
        base::TimeDelta(),
        base::BindRepeating(&ProxyResolverFactoryImplTest::OnFactoryIdle,
                            base::Unretained(this)));
  }

  ~ProxyResolverFactoryImplTest() override = default;

  void OnDisconnect() { waiter_.NotifyEvent(CONNECTION_ERROR); }

  void OnFakeProxyInstanceDestroyed() {
    instances_destroyed_++;
    waiter_.NotifyEvent(RESOLVER_DESTROYED);
  }

  void ReportResult(int32_t error) override {
    std::move(create_callback_).Run(error);
  }

  void Alert(const std::string& message) override {}

  void OnError(int32_t line_number, const std::string& message) override {}

  void ResolveDns(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<mojom::HostResolverRequestClient> client) override {}

  void set_idle_callback(base::OnceClosure callback) {
    idle_callback_ = std::move(callback);
  }

 protected:
  void OnFactoryIdle() {
    if (idle_callback_)
      std::move(idle_callback_).Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProxyResolverFactoryImpl> mock_factory_impl_;
  raw_ptr<TestProxyResolverFactory> mock_factory_;
  mojo::Remote<mojom::ProxyResolverFactory> factory_;

  int instances_destroyed_ = 0;
  net::CompletionOnceCallback create_callback_;

  base::OnceClosure idle_callback_;

  net::EventWaiter<Event> waiter_;
};

TEST_F(ProxyResolverFactoryImplTest, DisconnectProxyResolverClient) {
  mojo::Remote<mojom::ProxyResolver> proxy_resolver;
  mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client;
  mojo::Receiver<ProxyResolverFactoryRequestClient> client_receiver(
      this, client.InitWithNewPipeAndPassReceiver());
  factory_->CreateResolver(kScriptData,
                           proxy_resolver.BindNewPipeAndPassReceiver(),
                           std::move(client));
  proxy_resolver.set_disconnect_handler(base::BindOnce(
      &ProxyResolverFactoryImplTest::OnDisconnect, base::Unretained(this)));
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->requests_handled());
  net::TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  ASSERT_TRUE(mock_factory_->pending_request());
  *mock_factory_->pending_request()->resolver =
      std::make_unique<FakeProxyResolver>(base::BindOnce(
          &ProxyResolverFactoryImplTest::OnFakeProxyInstanceDestroyed,
          base::Unretained(this)));
  std::move(mock_factory_->pending_request()->callback).Run(net::OK);
  EXPECT_THAT(create_callback.WaitForResult(), IsOk());

  base::RunLoop wait_for_idle_loop;
  set_idle_callback(wait_for_idle_loop.QuitClosure());

  proxy_resolver.reset();
  waiter_.WaitForEvent(RESOLVER_DESTROYED);
  EXPECT_EQ(1, instances_destroyed_);

  wait_for_idle_loop.Run();
}

// Same as above, but disconnect the factory right after the CreateResolver
// call, which should not prevent the request from succeeding.
TEST_F(ProxyResolverFactoryImplTest, DisconnectProxyResolverFactory) {
  mojo::Remote<mojom::ProxyResolver> proxy_resolver;
  mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client;
  mojo::Receiver<ProxyResolverFactoryRequestClient> client_receiver(
      this, client.InitWithNewPipeAndPassReceiver());
  factory_->CreateResolver(kScriptData,
                           proxy_resolver.BindNewPipeAndPassReceiver(),
                           std::move(client));

  proxy_resolver.set_disconnect_handler(base::BindOnce(
      &ProxyResolverFactoryImplTest::OnDisconnect, base::Unretained(this)));
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->requests_handled());
  net::TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  ASSERT_TRUE(mock_factory_->pending_request());
  *mock_factory_->pending_request()->resolver =
      std::make_unique<FakeProxyResolver>(base::BindOnce(
          &ProxyResolverFactoryImplTest::OnFakeProxyInstanceDestroyed,
          base::Unretained(this)));
  std::move(mock_factory_->pending_request()->callback).Run(net::OK);
  EXPECT_THAT(create_callback.WaitForResult(), IsOk());

  base::RunLoop wait_for_idle_loop;
  set_idle_callback(wait_for_idle_loop.QuitClosure());

  proxy_resolver.reset();
  waiter_.WaitForEvent(RESOLVER_DESTROYED);
  EXPECT_EQ(1, instances_destroyed_);

  wait_for_idle_loop.Run();
}

TEST_F(ProxyResolverFactoryImplTest, Error) {
  mojo::Remote<mojom::ProxyResolver> proxy_resolver;
  mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client;
  mojo::Receiver<ProxyResolverFactoryRequestClient> client_receiver(
      this, client.InitWithNewPipeAndPassReceiver());
  factory_->CreateResolver(kScriptData,
                           proxy_resolver.BindNewPipeAndPassReceiver(),
                           std::move(client));
  proxy_resolver.set_disconnect_handler(base::BindOnce(
      &ProxyResolverFactoryImplTest::OnDisconnect, base::Unretained(this)));
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->requests_handled());
  net::TestCompletionCallback create_callback;
  create_callback_ = create_callback.callback();
  ASSERT_TRUE(mock_factory_->pending_request());
  std::move(mock_factory_->pending_request()->callback)
      .Run(net::ERR_PAC_SCRIPT_FAILED);
  EXPECT_THAT(create_callback.WaitForResult(),
              IsError(net::ERR_PAC_SCRIPT_FAILED));
}

TEST_F(ProxyResolverFactoryImplTest, DisconnectClientDuringResolverCreation) {
  mojo::Remote<mojom::ProxyResolver> proxy_resolver;
  mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client;
  mojo::Receiver<ProxyResolverFactoryRequestClient> client_receiver(
      this, client.InitWithNewPipeAndPassReceiver());
  factory_->CreateResolver(kScriptData,
                           proxy_resolver.BindNewPipeAndPassReceiver(),
                           std::move(client));
  proxy_resolver.set_disconnect_handler(base::BindOnce(
      &ProxyResolverFactoryImplTest::OnDisconnect, base::Unretained(this)));
  waiter_.WaitForEvent(RESOLVER_CREATED);
  EXPECT_EQ(0, instances_destroyed_);
  ASSERT_EQ(1u, mock_factory_->requests_handled());
  client_receiver.reset();
  waiter_.WaitForEvent(CONNECTION_ERROR);
}

}  // namespace proxy_resolver
