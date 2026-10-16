// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolver_apple.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

constexpr char kPacPath[] = "/proxy.pac";
constexpr char kHungPacPath[] = "/hung.pac";
constexpr char kNotFoundPacPath[] = "/missing.pac";
constexpr char kSlowEvalPacPath[] = "/slow_eval.pac";
constexpr char kValidPacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  return 'PROXY proxy.example.test:8080; DIRECT';\n"
    "}\n";
constexpr char kSlowEvalPacScript[] =
    "function FindProxyForURL(url, host) {\n"
    "  var end = Date.now() + 1500;\n"
    "  while (Date.now() < end) {}\n"
    "  return 'PROXY proxy.example.test:8080; DIRECT';\n"
    "}\n";

std::unique_ptr<test_server::HttpResponse> HandlePacRequest(
    const test_server::HttpRequest& request) {
  if (request.relative_url == kPacPath) {
    auto response = std::make_unique<test_server::BasicHttpResponse>();
    response->set_code(HTTP_OK);
    response->set_content_type("application/x-ns-proxy-autoconfig");
    response->set_content(kValidPacScript);
    return response;
  }
  if (request.relative_url == kSlowEvalPacPath) {
    auto response = std::make_unique<test_server::BasicHttpResponse>();
    response->set_code(HTTP_OK);
    response->set_content_type("application/x-ns-proxy-autoconfig");
    response->set_content(kSlowEvalPacScript);
    return response;
  }
  if (request.relative_url == kNotFoundPacPath) {
    auto response = std::make_unique<test_server::BasicHttpResponse>();
    response->set_code(HTTP_NOT_FOUND);
    return response;
  }
  if (request.relative_url == kHungPacPath) {
    return std::make_unique<test_server::HungResponse>();
  }
  return nullptr;
}

// On iOS 26+, `CFNetworkAgent` first attempts an opportunistic TLS handshake
// (with a 3.0s timeout) when fetching an `http://` PAC URL before falling back
// to plain HTTP. Because `EmbeddedTestServer` in HTTP mode waits for `\r\n\r\n`
// before parsing a request, a TLS `ClientHello` record (`0x16`) would otherwise
// stall each PAC fetch for 3.0s until `CFNetworkAgent`'s HTTPS upgrade task
// times out. Immediately closing sockets that send a TLS `ClientHello` allows
// `CFNetworkAgent` to fall back to plain HTTP without waiting for the timeout.
// This probe does not happen on macOS or iOS < 26, so the listener is a no-op
// there.
class RejectTlsClientHelloStreamSocket : public WrappedStreamSocket {
 public:
  explicit RejectTlsClientHelloStreamSocket(
      std::unique_ptr<StreamSocket> transport)
      : WrappedStreamSocket(std::move(transport)) {}

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    int rv = transport_->Read(
        buf, buf_len,
        base::BindOnce(&RejectTlsClientHelloStreamSocket::OnReadComplete,
                       weak_factory_.GetWeakPtr(), base::RetainedRef(buf),
                       std::move(callback)));
    if (rv > 0 && std::exchange(first_read_, false) &&
        IsTlsClientHello(buf, rv)) {
      Disconnect();
      return ERR_CONNECTION_CLOSED;
    }
    return rv;
  }

 private:
  static bool IsTlsClientHello(const IOBuffer* buf, int rv) {
    constexpr uint8_t kTlsHandshakeContentType = 0x16;
    return rv > 0 && buf->span()[0] == kTlsHandshakeContentType;
  }

  void OnReadComplete(scoped_refptr<IOBuffer> buf,
                      CompletionOnceCallback callback,
                      int rv) {
    if (rv > 0 && std::exchange(first_read_, false) &&
        IsTlsClientHello(buf.get(), rv)) {
      Disconnect();
      std::move(callback).Run(ERR_CONNECTION_CLOSED);
      return;
    }
    std::move(callback).Run(rv);
  }

  bool first_read_ = true;
  base::WeakPtrFactory<RejectTlsClientHelloStreamSocket> weak_factory_{this};
};

class RejectTlsUpgradeConnectionListener
    : public test_server::EmbeddedTestServerConnectionListener {
 public:
  RejectTlsUpgradeConnectionListener() = default;
  ~RejectTlsUpgradeConnectionListener() override = default;

  std::unique_ptr<StreamSocket> AcceptedSocket(
      std::unique_ptr<StreamSocket> socket) override {
    return std::make_unique<RejectTlsClientHelloStreamSocket>(
        std::move(socket));
  }

  void ReadFromSocket(const StreamSocket& socket, int rv) override {}
};

class ProxyResolverAppleTest : public ::testing::Test {
 protected:
  ProxyResolverAppleTest() {
    feature_list_.InitAndEnableFeature(features::kAsyncProxyResolverApple);
    test_server_.SetConnectionListener(&connection_listener_);
    test_server_.RegisterRequestMonitor(base::BindRepeating(
        &ProxyResolverAppleTest::OnServerRequest, base::Unretained(this)));
    test_server_.RegisterRequestHandler(base::BindRepeating(&HandlePacRequest));
  }

  void TearDown() override {
    if (test_server_.Started()) {
      ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
    }
  }

  void WaitForServerRequest(std::string_view relative_url) {
    base::RunLoop run_loop;
    {
      base::AutoLock lock(lock_);
      if (observed_requests_.contains(relative_url)) {
        return;
      }
      waiting_for_path_ = std::string(relative_url);
      quit_on_request_ = run_loop.QuitClosure();
    }
    run_loop.Run();
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::Lock lock_;
  std::set<std::string, std::less<>> observed_requests_ GUARDED_BY(lock_);
  std::string waiting_for_path_ GUARDED_BY(lock_);
  base::OnceClosure quit_on_request_ GUARDED_BY(lock_);
  RejectTlsUpgradeConnectionListener connection_listener_;
  EmbeddedTestServer test_server_;

 private:
  void OnServerRequest(const test_server::HttpRequest& request) {
    base::OnceClosure quit_closure;
    {
      base::AutoLock lock(lock_);
      observed_requests_.insert(request.relative_url);
      if (waiting_for_path_ == request.relative_url && quit_on_request_) {
        quit_closure = std::move(quit_on_request_);
      }
    }
    if (quit_closure) {
      std::move(quit_closure).Run();
    }
  }
};

// Tests that async `GetProxyForURL` returns `ERR_IO_PENDING` and resolves a PAC
// script served by `EmbeddedTestServer` without blocking the calling thread.
TEST_F(ProxyResolverAppleTest, AsyncGetProxyForURLResolvesPacScript) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kPacPath)),
                    &resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(resolver);

  ProxyInfo proxy_info;
  std::unique_ptr<ProxyResolver::Request> request;
  base::test::TestFuture<int> resolve_future;
  int rv = resolver->GetProxyForURL(
      GURL("http://target.example.test/page"), NetworkAnonymizationKey(),
      &proxy_info, resolve_future.GetCallback(), &request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_TRUE(request);
  EXPECT_FALSE(resolve_future.IsReady());

  EXPECT_EQ(OK, resolve_future.Get());
  EXPECT_FALSE(proxy_info.is_direct());
  EXPECT_EQ("PROXY proxy.example.test:8080;DIRECT", proxy_info.ToPacString());
}

// Tests that a failed PAC script fetch (HTTP 404) completes with `ERR_FAILED`.
TEST_F(ProxyResolverAppleTest, PacFetchFailureReturnsErrFailed) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kNotFoundPacPath)),
                    &resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(resolver);

  ProxyInfo proxy_info;
  std::unique_ptr<ProxyResolver::Request> request;
  base::test::TestFuture<int> resolve_future;
  int rv = resolver->GetProxyForURL(
      GURL("http://target.example.test/page"), NetworkAnonymizationKey(),
      &proxy_info, resolve_future.GetCallback(), &request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_TRUE(request);

  EXPECT_EQ(ERR_FAILED, resolve_future.Get());
}

// Tests that cancelling an in-flight `ProxyResolver::Request` against a hung
// PAC server returns immediately without hanging or invoking the callback, and
// leaves the shared PAC thread responsive for subsequent requests.
TEST_F(ProxyResolverAppleTest,
       CancelInFlightRequestDoesNotHangOrInvokeCallback) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> hung_resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kHungPacPath)),
                    &hung_resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(hung_resolver);

  ProxyInfo hung_proxy_info;
  std::unique_ptr<ProxyResolver::Request> hung_request;
  bool callback_invoked = false;
  int rv = hung_resolver->GetProxyForURL(
      GURL("http://target.example.test/page"), NetworkAnonymizationKey(),
      &hung_proxy_info,
      base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                     &callback_invoked),
      &hung_request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_TRUE(hung_request);

  // Wait until CFNetwork has actually requested the hung PAC script before
  // cancelling.
  WaitForServerRequest(kHungPacPath);

  hung_request.reset();
  hung_resolver.reset();
  EXPECT_FALSE(callback_invoked);

  // Verify the shared PAC thread is not stuck by resolving a valid PAC script.
  // Because `PacCFRunLoopThread` processes tasks in FIFO order,
  // `resolve_future` completing also guarantees `CancelOnPacThread` has
  // finished.
  std::unique_ptr<ProxyResolver> valid_resolver;
  ASSERT_EQ(OK,
            factory.CreateProxyResolver(
                PacFileData::FromURL(test_server_.GetURL(kPacPath)),
                &valid_resolver, CompletionOnceCallback(), &create_request));
  ProxyInfo valid_proxy_info;
  std::unique_ptr<ProxyResolver::Request> valid_request;
  base::test::TestFuture<int> resolve_future;
  ASSERT_EQ(ERR_IO_PENDING, valid_resolver->GetProxyForURL(
                                GURL("http://target.example.test/page"),
                                NetworkAnonymizationKey(), &valid_proxy_info,
                                resolve_future.GetCallback(), &valid_request,
                                NetLogWithSource()));
  EXPECT_EQ(OK, resolve_future.Get());
  EXPECT_EQ("PROXY proxy.example.test:8080;DIRECT",
            valid_proxy_info.ToPacString());
  EXPECT_FALSE(callback_invoked);

  // Now that `resolve_future` has completed, `~Context()` for `hung_request`
  // has already run on `PacCFRunLoopThread`. Explicitly close the hung socket
  // and allow CFNetwork to process the close to verify CFNetwork does not
  // touch `client_context.info` after `CFRunLoopSourceInvalidate`.
  ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  base::RunLoop wait_for_socket_close;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, wait_for_socket_close.QuitClosure(), base::Milliseconds(200));
  wait_for_socket_close.Run();
  EXPECT_FALSE(callback_invoked);
}

// Tests that cancelling a `ProxyResolver::Request` immediately after
// `GetProxyForURL` returns `ERR_IO_PENDING` (before spinning the message loop,
// which may hit the `cancelled_` early return in `StartOnPacThread` or cancel
// after `StartOnPacThread` has run) does not crash or invoke the callback.
TEST_F(ProxyResolverAppleTest, CancelImmediatelyBeforePacThreadDispatch) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kPacPath)),
                    &resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(resolver);

  ProxyInfo proxy_info;
  std::unique_ptr<ProxyResolver::Request> request;
  bool callback_invoked = false;
  int rv = resolver->GetProxyForURL(
      GURL("http://target.example.test/page"), NetworkAnonymizationKey(),
      &proxy_info,
      base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                     &callback_invoked),
      &request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_TRUE(request);

  // Synchronously cancel before running the message loop.
  request.reset();

  // Flush `PacCFRunLoopThread` by resolving a second request on `resolver`.
  ProxyInfo second_info;
  std::unique_ptr<ProxyResolver::Request> second_request;
  base::test::TestFuture<int> second_future;
  ASSERT_EQ(ERR_IO_PENDING,
            resolver->GetProxyForURL(GURL("http://target.example.test/second"),
                                     NetworkAnonymizationKey(), &second_info,
                                     second_future.GetCallback(),
                                     &second_request, NetLogWithSource()));
  EXPECT_EQ(OK, second_future.Get());
  EXPECT_EQ("PROXY proxy.example.test:8080;DIRECT", second_info.ToPacString());
  EXPECT_FALSE(callback_invoked);
}

// Tests that cancelling a request while the PAC script may still be evaluating
// on `PacCFRunLoopThread` does not block the calling (IO) thread and drops any
// late result.
TEST_F(ProxyResolverAppleTest,
       CancelDuringSlowPacEvaluationReturnsImmediately) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kSlowEvalPacPath)),
                    &resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(resolver);

  ProxyInfo proxy_info;
  std::unique_ptr<ProxyResolver::Request> request;
  bool callback_invoked = false;
  int rv = resolver->GetProxyForURL(
      GURL("http://target.example.test/page"), NetworkAnonymizationKey(),
      &proxy_info,
      base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                     &callback_invoked),
      &request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_TRUE(request);

  WaitForServerRequest(kSlowEvalPacPath);
  // Allow time for the loopback response to be delivered so the PAC script may
  // still be evaluating inside `FindProxyForURL` (for 1500 ms) when `reset()`
  // is called on the IO thread.
  base::RunLoop give_pac_thread_head_start;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, give_pac_thread_head_start.QuitClosure(),
      base::Milliseconds(50));
  give_pac_thread_head_start.Run();

  const base::TimeTicks cancel_start = base::TimeTicks::Now();
  request.reset();
  const base::TimeDelta cancel_duration = base::TimeTicks::Now() - cancel_start;
  EXPECT_LT(cancel_duration, base::Seconds(1));
  EXPECT_FALSE(callback_invoked);

  // Wait past the 1.5 s PAC evaluation window to verify that any late result
  // after cancellation is dropped without invoking the callback.
  base::RunLoop wait_for_late_eval;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, wait_for_late_eval.QuitClosure(), base::Seconds(2));
  wait_for_late_eval.Run();
  EXPECT_FALSE(callback_invoked);
}

// Tests that destroying `ProxyResolverApple` while multiple `Request`s are
// still alive cancels all active requests cleanly via `~ProxyResolverApple()`.
TEST_F(ProxyResolverAppleTest, DestroyResolverBeforeRequestCancelsCleanly) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kHungPacPath)),
                    &resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(resolver);

  ProxyInfo first_info;
  std::unique_ptr<ProxyResolver::Request> first_request;
  bool first_callback_invoked = false;
  ASSERT_EQ(
      ERR_IO_PENDING,
      resolver->GetProxyForURL(
          GURL("http://target.example.test/first"), NetworkAnonymizationKey(),
          &first_info,
          base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                         &first_callback_invoked),
          &first_request, NetLogWithSource()));
  ASSERT_TRUE(first_request);

  ProxyInfo second_info;
  std::unique_ptr<ProxyResolver::Request> second_request;
  bool second_callback_invoked = false;
  ASSERT_EQ(
      ERR_IO_PENDING,
      resolver->GetProxyForURL(
          GURL("http://target.example.test/second"), NetworkAnonymizationKey(),
          &second_info,
          base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                         &second_callback_invoked),
          &second_request, NetLogWithSource()));
  ASSERT_TRUE(second_request);

  WaitForServerRequest(kHungPacPath);

  // Destroy the resolver before the requests to exercise `~ProxyResolverApple`
  // with multiple entries in `active_requests_`.
  resolver.reset();
  first_request.reset();
  second_request.reset();
  EXPECT_FALSE(first_callback_invoked);
  EXPECT_FALSE(second_callback_invoked);
}

// Tests that cancelling one in-flight request while another is running allows
// the remaining request to complete normally.
TEST_F(ProxyResolverAppleTest, ConcurrentRequestsCancelOneCompletesOther) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> hung_resolver;
  std::unique_ptr<ProxyResolver> valid_resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kHungPacPath)),
                    &hung_resolver, CompletionOnceCallback(), &create_request));
  ASSERT_EQ(OK,
            factory.CreateProxyResolver(
                PacFileData::FromURL(test_server_.GetURL(kPacPath)),
                &valid_resolver, CompletionOnceCallback(), &create_request));

  ProxyInfo hung_info;
  std::unique_ptr<ProxyResolver::Request> hung_request;
  bool hung_callback_invoked = false;
  ASSERT_EQ(
      ERR_IO_PENDING,
      hung_resolver->GetProxyForURL(
          GURL("http://target.example.test/hung"), NetworkAnonymizationKey(),
          &hung_info,
          base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                         &hung_callback_invoked),
          &hung_request, NetLogWithSource()));

  WaitForServerRequest(kHungPacPath);

  ProxyInfo valid_info;
  std::unique_ptr<ProxyResolver::Request> valid_request;
  base::test::TestFuture<int> valid_future;
  ASSERT_EQ(ERR_IO_PENDING, valid_resolver->GetProxyForURL(
                                GURL("http://target.example.test/ok"),
                                NetworkAnonymizationKey(), &valid_info,
                                valid_future.GetCallback(), &valid_request,
                                NetLogWithSource()));

  // Cancel the hung request while the valid request is in flight.
  hung_request.reset();

  EXPECT_EQ(OK, valid_future.Get());
  EXPECT_EQ("PROXY proxy.example.test:8080;DIRECT", valid_info.ToPacString());
  EXPECT_FALSE(hung_callback_invoked);
}

// Tests that issuing multiple concurrent requests against the same
// `ProxyResolverApple` instance and cancelling one leaves the remaining
// request in `active_requests_` to complete normally.
TEST_F(ProxyResolverAppleTest,
       ConcurrentRequestsOnSameResolverCancelOneCompletesOther) {
  ASSERT_TRUE(test_server_.Start());

  ProxyResolverFactoryApple factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> create_request;
  ASSERT_EQ(OK, factory.CreateProxyResolver(
                    PacFileData::FromURL(test_server_.GetURL(kPacPath)),
                    &resolver, CompletionOnceCallback(), &create_request));
  ASSERT_TRUE(resolver);

  ProxyInfo first_info;
  std::unique_ptr<ProxyResolver::Request> first_request;
  bool first_callback_invoked = false;
  ASSERT_EQ(
      ERR_IO_PENDING,
      resolver->GetProxyForURL(
          GURL("http://target.example.test/first"), NetworkAnonymizationKey(),
          &first_info,
          base::BindOnce([](bool* invoked, int /*error*/) { *invoked = true; },
                         &first_callback_invoked),
          &first_request, NetLogWithSource()));

  ProxyInfo second_info;
  std::unique_ptr<ProxyResolver::Request> second_request;
  base::test::TestFuture<int> second_future;
  ASSERT_EQ(ERR_IO_PENDING,
            resolver->GetProxyForURL(GURL("http://target.example.test/second"),
                                     NetworkAnonymizationKey(), &second_info,
                                     second_future.GetCallback(),
                                     &second_request, NetLogWithSource()));

  // Both requests are now registered in `resolver->active_requests_`. Cancel
  // the first and verify the second still completes.
  first_request.reset();

  EXPECT_EQ(OK, second_future.Get());
  EXPECT_EQ("PROXY proxy.example.test:8080;DIRECT", second_info.ToPacString());
  EXPECT_FALSE(first_callback_invoked);
}

// Tests that destroying `ConfiguredProxyResolutionService` created via
// `CreateUsingSystemProxyResolver` while a PAC resolution is hung completes
// immediately without blocking shutdown.
TEST_F(ProxyResolverAppleTest,
       DestroyConfiguredProxyResolutionServiceWithHungRequestDoesNotHang) {
  ASSERT_TRUE(test_server_.Start());

  ProxyConfig config =
      ProxyConfig::CreateFromCustomPacURL(test_server_.GetURL(kHungPacPath));
  auto config_service = std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ConfiguredProxyResolutionService> service =
      ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(config_service),
          /*host_resolver_for_override_rules=*/nullptr,
          /*net_log=*/nullptr, /*quick_check_enabled=*/false);
  ASSERT_TRUE(service);

  ProxyInfo proxy_info;
  std::unique_ptr<ProxyResolutionRequest> request;
  base::test::TestFuture<int> resolve_future;
  int rv = service->ResolveProxy(
      GURL("http://target.example.test/page"), "GET", NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &proxy_info, resolve_future.GetCallback(),
      &request, NetLogWithSource(), DEFAULT_PRIORITY);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_TRUE(request);

  // Wait until CFNetwork has actually issued the PAC fetch before tearing down
  // the service.
  WaitForServerRequest(kHungPacPath);

  const base::TimeTicks destroy_start = base::TimeTicks::Now();
  service.reset();
  request.reset();
  EXPECT_LT(base::TimeTicks::Now() - destroy_start, base::Seconds(1));
}

// Tests that when `kAsyncProxyResolverApple` is disabled,
// `CreateUsingSystemProxyResolver` resolves a PAC script via
// `MultiThreadedProxyResolver` (constructing and querying `ProxyResolverApple`
// on a worker thread) and tears down cleanly on the origin sequence.
TEST_F(ProxyResolverAppleTest,
       LegacySyncSystemProxyResolverResolvesAndDestroysCleanly) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kAsyncProxyResolverApple);

  ASSERT_TRUE(test_server_.Start());

  ProxyConfig config =
      ProxyConfig::CreateFromCustomPacURL(test_server_.GetURL(kPacPath));
  auto config_service = std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ConfiguredProxyResolutionService> service =
      ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(config_service),
          /*host_resolver_for_override_rules=*/nullptr,
          /*net_log=*/nullptr, /*quick_check_enabled=*/false);
  ASSERT_TRUE(service);

  ProxyInfo proxy_info;
  std::unique_ptr<ProxyResolutionRequest> request;
  base::test::TestFuture<int> resolve_future;
  int rv = service->ResolveProxy(
      GURL("http://target.example.test/page"), "GET", NetworkAnonymizationKey(),
      handles::kInvalidNetworkHandle, &proxy_info, resolve_future.GetCallback(),
      &request, NetLogWithSource(), DEFAULT_PRIORITY);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, resolve_future.Get());
  EXPECT_EQ("PROXY proxy.example.test:8080;DIRECT", proxy_info.ToPacString());

  service.reset();
  request.reset();
}

}  // namespace
}  // namespace net
