// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/linux_system_proxy_resolver_mojo.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/linux/linux_proxy_resolution_status.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolver.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

class MockLinuxSystemProxyResolver
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  MockLinuxSystemProxyResolver(
      net::ProxyList proxy_list,
      std::optional<proxy_resolver::mojom::LinuxProxyStatus> failure,
      bool drop_callback)
      : proxy_list_(std::move(proxy_list)),
        failure_(failure),
        drop_callback_(drop_callback) {}
  ~MockLinuxSystemProxyResolver() override = default;

  // proxy_resolver::mojom::SystemProxyResolver implementation:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override {
    got_request_ = true;
    if (drop_callback_) {
      // Hold onto the callback without replying. It is destroyed only after
      // the receiver is closed.
      dropped_callback_ = std::move(callback);
      return;
    }
    auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
    status->is_success = !failure_.has_value();
    status->os_error = 0;
    status->linux_proxy_status = failure_;

    // Simulate the asynchronous nature of this call.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), proxy_list_, std::move(status)));
  }

  bool got_request() const { return got_request_; }

 private:
  net::ProxyList proxy_list_;
  std::optional<proxy_resolver::mojom::LinuxProxyStatus> failure_;
  bool drop_callback_;
  bool got_request_ = false;
  GetProxyForUrlCallback dropped_callback_;
};

// Starts a proxy resolution on construction and records its result.
class TestLinuxSystemProxyResolutionRequest {
 public:
  explicit TestLinuxSystemProxyResolutionRequest(
      net::LinuxSystemProxyResolver* resolver) {
    request_ = resolver->GetProxyForUrl(
        GURL("https://example.test/"),
        base::BindOnce(
            &TestLinuxSystemProxyResolutionRequest::ProxyResolutionComplete,
            weak_ptr_factory_.GetWeakPtr()));
    EXPECT_TRUE(request_);
  }

  void WaitForProxyResolutionComplete() { EXPECT_TRUE(result_.Wait()); }

  void DeleteRequest() { request_.reset(); }

  const net::ProxyList& proxy_list() { return result_.Get<0>(); }
  std::optional<net::LinuxProxyResolutionStatus> status() {
    if (!result_.IsReady()) {
      return std::nullopt;
    }
    return result_.Get<1>();
  }

 private:
  void ProxyResolutionComplete(const net::ProxyList& proxy_list,
                               net::LinuxProxyResolutionStatus linux_status) {
    // Mirror net::LinuxSystemProxyResolutionRequest, which destroys the
    // request handle from within the callback.
    EXPECT_TRUE(request_);
    DeleteRequest();
    result_.SetValue(proxy_list, linux_status);
  }

  std::unique_ptr<net::LinuxSystemProxyResolver::Request> request_;
  base::test::TestFuture<net::ProxyList, net::LinuxProxyResolutionStatus>
      result_;
  base::WeakPtrFactory<TestLinuxSystemProxyResolutionRequest> weak_ptr_factory_{
      this};
};

}  // namespace

class LinuxSystemProxyResolverMojoTest : public testing::Test {
 public:
  void CreateResolver(net::ProxyList proxy_list = net::ProxyList(),
                      std::optional<proxy_resolver::mojom::LinuxProxyStatus>
                          failure = std::nullopt,
                      bool drop_callback = false) {
    mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver> remote;
    auto mock = std::make_unique<MockLinuxSystemProxyResolver>(
        std::move(proxy_list), failure, drop_callback);
    mock_ = mock.get();
    receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::move(mock), remote.InitWithNewPipeAndPassReceiver());
    resolver_ =
        std::make_unique<LinuxSystemProxyResolverMojo>(std::move(remote));
  }

  net::LinuxSystemProxyResolver* proxy_resolver() { return resolver_.get(); }

  MockLinuxSystemProxyResolver* mock() { return mock_; }

  // Closes the browser end of the pipe, destroying the mock.
  void CloseReceiver() {
    mock_ = nullptr;
    receiver_ref_->Close();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockLinuxSystemProxyResolver> mock_ = nullptr;
  mojo::SelfOwnedReceiverRef<proxy_resolver::mojom::SystemProxyResolver>
      receiver_ref_;
  std::unique_ptr<net::LinuxSystemProxyResolver> resolver_;
};

TEST_F(LinuxSystemProxyResolverMojoTest, ProxyResolutionBasic) {
  net::ProxyList expected;
  expected.AddProxyServer(
      net::PacResultElementToProxyServer("HTTPS foopy:8443"));
  CreateResolver(expected);

  TestLinuxSystemProxyResolutionRequest request(proxy_resolver());
  request.WaitForProxyResolutionComplete();
  EXPECT_EQ(net::LinuxProxyResolutionStatus::kOk, request.status());
  EXPECT_TRUE(expected.Equals(request.proxy_list()));
}

TEST_F(LinuxSystemProxyResolverMojoTest, ProxyResolutionFailure) {
  CreateResolver(net::ProxyList(),
                 proxy_resolver::mojom::LinuxProxyStatus::kPortalUnavailable);

  TestLinuxSystemProxyResolutionRequest request(proxy_resolver());
  request.WaitForProxyResolutionComplete();
  EXPECT_EQ(net::LinuxProxyResolutionStatus::kPortalUnavailable,
            request.status());
  EXPECT_TRUE(request.proxy_list().IsEmpty());
}

TEST_F(LinuxSystemProxyResolverMojoTest, ProxyResolutionCanceled) {
  CreateResolver();
  TestLinuxSystemProxyResolutionRequest request(proxy_resolver());
  request.DeleteRequest();

  // This shouldn't crash and the result callback should never run. Post a
  // sentinel task and wait for it to ensure all previously posted tasks
  // (including the Mojo callback) have had a chance to run.
  bool sentinel_ran = false;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce([](bool* flag) { *flag = true; }, &sentinel_ran));
  ASSERT_TRUE(base::test::RunUntil([&]() { return sentinel_ran; }));
  EXPECT_FALSE(request.status());
}

TEST_F(LinuxSystemProxyResolverMojoTest, RemoteDisconnectAborts) {
  CreateResolver(net::ProxyList(), std::nullopt, /*drop_callback=*/true);

  TestLinuxSystemProxyResolutionRequest request(proxy_resolver());
  ASSERT_TRUE(base::test::RunUntil([&]() { return mock()->got_request(); }));
  ASSERT_FALSE(request.status());

  // Simulate the browser end going away without replying. The request must
  // complete with kAborted rather than stay pending forever.
  CloseReceiver();
  request.WaitForProxyResolutionComplete();
  EXPECT_EQ(net::LinuxProxyResolutionStatus::kAborted, request.status());
  EXPECT_TRUE(request.proxy_list().IsEmpty());
}

}  // namespace network
