// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mac_system_proxy_resolver_mojo.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_anonymization_key.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

class MockMacSystemProxyResolver
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  MockMacSystemProxyResolver() = default;
  ~MockMacSystemProxyResolver() override = default;

  // proxy_resolver::mojom::SystemProxyResolver implementation:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override {
    auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
    status->is_success = true;
    status->os_error = 0;
    status->mac_proxy_status = net::MacProxyResolutionStatus::kOk;

    // Simulate asynchronous nature of this call by posting a task to run the
    // callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::ProxyList(),
                                  std::move(status)));
  }
};

class MockMacSystemProxyResolutionRequest
    : public net::MacSystemProxyResolutionRequest {
 public:
  explicit MockMacSystemProxyResolutionRequest(
      net::MacSystemProxyResolver* resolver)
      : net::MacSystemProxyResolutionRequest(nullptr,
                                             GURL(),
                                             std::string(),
                                             net::NetworkAnonymizationKey(),
                                             nullptr,
                                             base::DoNothing(),
                                             net::NetLogWithSource(),
                                             *resolver) {
    EXPECT_TRUE(GetProxyResolutionRequestForTesting());
  }
  ~MockMacSystemProxyResolutionRequest() override = default;

  void WaitForProxyResolutionComplete() { loop_.Run(); }

  void ProxyResolutionComplete(const net::ProxyList& proxy_list,
                               net::MacProxyResolutionStatus mac_status,
                               int os_error) override {
    EXPECT_TRUE(GetProxyResolutionRequestForTesting());
    DeleteRequest();
    loop_.Quit();
  }

  void DeleteRequest() { ResetProxyResolutionRequestForTesting(); }

 private:
  base::RunLoop loop_;
};

}  // namespace

class MacSystemProxyResolverMojoTest : public testing::Test {
 public:
  void SetUp() override {
    mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver> remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<MockMacSystemProxyResolver>(),
                                remote.InitWithNewPipeAndPassReceiver());
    mac_system_proxy_resolver_mojo_ =
        std::make_unique<MacSystemProxyResolverMojo>(std::move(remote));
  }

  net::MacSystemProxyResolver* proxy_resolver() {
    return mac_system_proxy_resolver_mojo_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::MacSystemProxyResolver> mac_system_proxy_resolver_mojo_;
};

TEST_F(MacSystemProxyResolverMojoTest, ProxyResolutionBasic) {
  MockMacSystemProxyResolutionRequest request(proxy_resolver());
  request.WaitForProxyResolutionComplete();
}

TEST_F(MacSystemProxyResolverMojoTest, ProxyResolutionCanceled) {
  MockMacSystemProxyResolutionRequest request(proxy_resolver());
  request.DeleteRequest();

  // This shouldn't crash and there should never be a callback to
  // ProxyResolutionComplete(). Post a sentinel task and wait for it to
  // ensure all previously posted tasks (including the Mojo callback) have
  // had a chance to run.
  bool sentinel_ran = false;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce([](bool* flag) { *flag = true; }, &sentinel_ran));
  ASSERT_TRUE(base::test::RunUntil([&]() { return sentinel_ran; }));
}

}  // namespace network
