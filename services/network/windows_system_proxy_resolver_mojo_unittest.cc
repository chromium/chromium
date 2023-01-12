// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/windows_system_proxy_resolver_mojo.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/winhttp_status.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

class MockWindowsSystemProxyResolver
    : public proxy_resolver_win::mojom::WindowsSystemProxyResolver {
 public:
  MockWindowsSystemProxyResolver() = default;
  ~MockWindowsSystemProxyResolver() override = default;

  // proxy_resolver_win::mojom::WindowsSystemProxyResolver implementation:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override {
    // Simulate asynchronous nature of this call by posting a task to run the
    // callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::ProxyList(),
                                  net::WinHttpStatus::kOk, 0));
  }
};

class MockWindowsSystemProxyResolutionRequest
    : public net::WindowsSystemProxyResolutionRequest {
 public:
  MockWindowsSystemProxyResolutionRequest(
      net::WindowsSystemProxyResolver* resolver)
      : net::WindowsSystemProxyResolutionRequest(nullptr,
                                                 GURL(),
                                                 std::string(),
                                                 nullptr,
                                                 base::DoNothing(),
                                                 net::NetLogWithSource(),
                                                 resolver) {
    EXPECT_TRUE(GetProxyResolutionRequestForTesting());
  }
  ~MockWindowsSystemProxyResolutionRequest() override = default;

  void WaitForProxyResolutionComplete() { loop_.Run(); }

  void ProxyResolutionComplete(const net::ProxyList& proxy_list,
                               net::WinHttpStatus winhttp_status,
                               int windows_error) override {
    EXPECT_TRUE(GetProxyResolutionRequestForTesting());
    DeleteRequest();
    loop_.Quit();
  }

  void DeleteRequest() { ResetProxyResolutionRequestForTesting(); }

 private:
  base::RunLoop loop_;
};

}  // namespace

class WindowsSystemProxyResolverMojoTest : public testing::Test {
 public:
  void SetUp() override {
    mojo::PendingRemote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
        remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockWindowsSystemProxyResolver>(),
        remote.InitWithNewPipeAndPassReceiver());
    windows_system_proxy_resolver_mojo_ =
        std::make_unique<WindowsSystemProxyResolverMojo>(std::move(remote));
  }

  net::WindowsSystemProxyResolver* proxy_resolver() {
    return windows_system_proxy_resolver_mojo_.get();
  }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::WindowsSystemProxyResolver>
      windows_system_proxy_resolver_mojo_;
};

TEST_F(WindowsSystemProxyResolverMojoTest, ProxyResolutionBasic) {
  MockWindowsSystemProxyResolutionRequest request(proxy_resolver());
  request.WaitForProxyResolutionComplete();
}

TEST_F(WindowsSystemProxyResolverMojoTest, ProxyResolutionCanceled) {
  MockWindowsSystemProxyResolutionRequest request(proxy_resolver());
  request.DeleteRequest();

  // This shouldn't crash and there should never be a callback to
  // ProxyResolutionComplete().
  RunUntilIdle();
}

}  // namespace network
