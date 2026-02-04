// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/mac_system_proxy_resolver_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/proxy_resolver_mac/mac_api_wrapper/mac_api_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace proxy_resolver_mac {
namespace {

// Mock implementation of MacAPIWrapper for unit testing.
// TODO(crbug.com/442313607): The test configuration and verification methods
// below will be used once the real implementation is available.
class MockMacAPIWrapper : public MacAPIWrapper {
 public:
  MockMacAPIWrapper() = default;
  ~MockMacAPIWrapper() override = default;

  MockMacAPIWrapper(const MockMacAPIWrapper&) = delete;
  MockMacAPIWrapper& operator=(const MockMacAPIWrapper&) = delete;

  // MacAPIWrapper implementation:
  base::apple::ScopedCFTypeRef<CFDictionaryRef> CopyProxies() override {
    ++copy_proxies_call_count_;
    return copy_proxies_result_;
  }

  base::apple::ScopedCFTypeRef<CFArrayRef> CopyProxiesForURL(
      const GURL& url,
      CFDictionaryRef proxy_settings) override {
    ++copy_proxies_for_url_call_count_;
    last_url_ = url;
    return copy_proxies_for_url_result_;
  }

  // Test configuration methods:

  // Sets the CFDictionaryRef to return from CopyProxies(). Pass nullptr to
  // simulate failure.
  void set_copy_proxies_result(
      base::apple::ScopedCFTypeRef<CFDictionaryRef> result) {
    copy_proxies_result_ = std::move(result);
  }

  // Sets the result for CopyProxiesForURL().
  void set_copy_proxies_for_url_result(
      base::apple::ScopedCFTypeRef<CFArrayRef> result) {
    copy_proxies_for_url_result_ = std::move(result);
  }

  // Returns the number of times CopyProxies() was called.
  int copy_proxies_call_count() const { return copy_proxies_call_count_; }

  // Returns the number of times CopyProxiesForURL() was called.
  int copy_proxies_for_url_call_count() const {
    return copy_proxies_for_url_call_count_;
  }

  // Returns the last URL passed to CopyProxiesForURL().
  const std::optional<GURL>& last_url() const { return last_url_; }

 private:
  base::apple::ScopedCFTypeRef<CFDictionaryRef> copy_proxies_result_;
  base::apple::ScopedCFTypeRef<CFArrayRef> copy_proxies_for_url_result_;
  int copy_proxies_call_count_ = 0;
  int copy_proxies_for_url_call_count_ = 0;
  std::optional<GURL> last_url_;
};

class MacSystemProxyResolverImplTest : public testing::Test {
 public:
  MacSystemProxyResolverImplTest() = default;
  ~MacSystemProxyResolverImplTest() override = default;

 protected:
  void CreateResolver(std::unique_ptr<MockMacAPIWrapper> mock_wrapper) {
    resolver_impl_ = std::make_unique<MacSystemProxyResolverImpl>(
        resolver_.BindNewPipeAndPassReceiver(), std::move(mock_wrapper));
  }

  void GetProxyForUrl(const GURL& url) {
    base::RunLoop run_loop;
    resolver_->GetProxyForUrl(
        url, base::BindOnce(
                 &MacSystemProxyResolverImplTest::OnGetProxyForUrlComplete,
                 base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnGetProxyForUrlComplete(
      base::OnceClosure quit_closure,
      const net::ProxyList& proxy_list,
      proxy_resolver::mojom::SystemProxyResolutionStatusPtr status) {
    last_proxy_list_ = proxy_list;
    last_status_ = std::move(status);
    std::move(quit_closure).Run();
  }

  const net::ProxyList& last_proxy_list() const { return last_proxy_list_; }

  const proxy_resolver::mojom::SystemProxyResolutionStatusPtr& last_status()
      const {
    return last_status_;
  }

  bool IsResolverBound() const { return resolver_.is_bound(); }

  bool IsResolverConnected() const { return resolver_.is_connected(); }

  void ResetResolverImpl() { resolver_impl_.reset(); }

  void ResetResolverRemote() { resolver_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<proxy_resolver::mojom::SystemProxyResolver> resolver_;
  std::unique_ptr<MacSystemProxyResolverImpl> resolver_impl_;

  net::ProxyList last_proxy_list_;
  proxy_resolver::mojom::SystemProxyResolutionStatusPtr last_status_;
};

// Tests that GetProxyForUrl returns an error when CopyProxies() fails.
// TODO(crbug.com/442313607): Update or remove this test once the macOS system
// proxy resolver implementation is available.
TEST_F(MacSystemProxyResolverImplTest, GetProxyForUrlFailsOnCopyProxiesError) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  // Don't set any result - CopyProxies() will return nullptr.
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  // Since the skeleton returns NOTIMPLEMENTED error, verify error status.
  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kSystemConfigurationError);
  EXPECT_TRUE(last_proxy_list().IsEmpty());
}

// Tests that the resolver can be created and destroyed without crashing.
TEST_F(MacSystemProxyResolverImplTest, CreateAndDestroy) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  CreateResolver(std::move(mock));
  ResetResolverImpl();
  // Test passes if no crash occurs.
}

// Tests that the Mojo connection works correctly.
TEST_F(MacSystemProxyResolverImplTest, MojoConnectionWorks) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  CreateResolver(std::move(mock));

  EXPECT_TRUE(IsResolverBound());
  EXPECT_TRUE(IsResolverConnected());
}

// Tests multiple sequential GetProxyForUrl calls.
TEST_F(MacSystemProxyResolverImplTest, MultipleGetProxyForUrlCalls) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example1.com"));
  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);

  GetProxyForUrl(GURL("https://example2.com"));
  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);

  GetProxyForUrl(GURL("https://example3.com"));
  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
}

// Tests that disconnecting the remote doesn't crash.
TEST_F(MacSystemProxyResolverImplTest, DisconnectRemote) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  CreateResolver(std::move(mock));

  ResetResolverRemote();
  // Destructor will clean up. Test passes if no crash occurs.
}

}  // namespace
}  // namespace proxy_resolver_mac
