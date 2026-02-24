// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/mac_system_proxy_resolver_impl.h"

#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
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

// Creates a CFDictionary representing a DIRECT (kCFProxyTypeNone) proxy entry
// as returned by CFNetworkCopyProxiesForURL.
base::apple::ScopedCFTypeRef<CFDictionaryRef>
CreateProxyDictionaryForProxyTypeNone() {
  const void* keys[] = {kCFProxyTypeKey};
  const void* values[] = {kCFProxyTypeNone};
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

// Creates a CFDictionary representing a proxy entry as returned by
// CFNetworkCopyProxiesForURL. `proxy_type` is one of the kCFProxyType*
// constants (e.g. kCFProxyTypeHTTP, kCFProxyTypeSOCKS). `host` is the proxy
// hostname and `port` is the proxy port number. Must not be called with
// kCFProxyTypeNone; use CreateProxyDictionaryForProxyTypeNone() instead.
base::apple::ScopedCFTypeRef<CFDictionaryRef>
CreateProxyDictionary(CFStringRef proxy_type, std::string_view host, int port) {
  CHECK(!CFEqual(proxy_type, kCFProxyTypeNone));

  auto host_ref = base::SysUTF8ToCFStringRef(host);
  base::apple::ScopedCFTypeRef<CFNumberRef> port_ref(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &port));

  const void* keys[] = {kCFProxyTypeKey, kCFProxyHostNameKey,
                        kCFProxyPortNumberKey};
  const void* values[] = {proxy_type, host_ref.get(), port_ref.get()};
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

// Creates a PAC URL proxy dictionary entry. `pac_url` is the URL of the PAC
// script.
base::apple::ScopedCFTypeRef<CFDictionaryRef> CreatePacUrlProxyDictionary(
    std::string_view pac_url) {
  auto url_string = base::SysUTF8ToCFStringRef(pac_url);
  base::apple::ScopedCFTypeRef<CFURLRef> url_ref(
      CFURLCreateWithString(kCFAllocatorDefault, url_string.get(), nullptr));

  const void* keys[] = {kCFProxyTypeKey, kCFProxyAutoConfigurationURLKey};
  const void* values[] = {kCFProxyTypeAutoConfigurationURL, url_ref.get()};
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

// Creates a PAC JavaScript proxy dictionary entry. `script` is the inline
// JavaScript source for proxy auto-configuration.
base::apple::ScopedCFTypeRef<CFDictionaryRef>
CreatePacJavaScriptProxyDictionary(std::string_view script) {
  auto script_string = base::SysUTF8ToCFStringRef(script);

  const void* keys[] = {kCFProxyTypeKey,
                        kCFProxyAutoConfigurationJavaScriptKey};
  const void* values[] = {kCFProxyTypeAutoConfigurationJavaScript,
                          script_string.get()};
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

// Helper to create a CFArray of proxy dictionaries.
base::apple::ScopedCFTypeRef<CFArrayRef> CreateProxyArray(
    std::initializer_list<CFDictionaryRef> dicts) {
  return base::apple::ScopedCFTypeRef<CFArrayRef>(
      CFArrayCreate(kCFAllocatorDefault,
                    reinterpret_cast<const void**>(
                        const_cast<CFDictionaryRef*>(dicts.begin())),
                    dicts.size(), &kCFTypeArrayCallBacks));
}

// Helper to create an empty but valid proxy settings dictionary.
base::apple::ScopedCFTypeRef<CFDictionaryRef> CreateEmptyProxySettings() {
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

// Mock implementation of MacAPIWrapper for unit testing.
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
TEST_F(MacSystemProxyResolverImplTest, GetProxyForUrlFailsOnCopyProxiesError) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  // Don't set any result - CopyProxies() will return nullptr.
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kSystemConfigurationError);
  EXPECT_TRUE(last_proxy_list().IsEmpty());
}

// Tests that GetProxyForUrl returns an error when CopyProxiesForURL() fails.
TEST_F(MacSystemProxyResolverImplTest,
       GetProxyForUrlFailsOnCopyProxiesForURLError) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());
  // Don't set CopyProxiesForURL result - it will return nullptr.
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kCFNetworkResolutionError);
  EXPECT_TRUE(last_proxy_list().IsEmpty());
}

// Tests successful HTTP proxy resolution.
TEST_F(MacSystemProxyResolverImplTest, SuccessfulHttpProxyResolution) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto proxy_dict = CreateProxyDictionary(kCFProxyTypeHTTP, "proxy.test", 8080);
  mock->set_copy_proxies_for_url_result(CreateProxyArray({proxy_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kOk);
  EXPECT_FALSE(last_proxy_list().IsEmpty());
  EXPECT_EQ(last_proxy_list().ToDebugString(), "PROXY proxy.test:8080");
}

// Tests resolution returning DIRECT (no proxy).
TEST_F(MacSystemProxyResolverImplTest, DirectProxyResolution) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto proxy_dict = CreateProxyDictionaryForProxyTypeNone();
  mock->set_copy_proxies_for_url_result(CreateProxyArray({proxy_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kOk);
  EXPECT_FALSE(last_proxy_list().IsEmpty());
  EXPECT_EQ(last_proxy_list().ToDebugString(), "DIRECT");
}

// Tests resolution with a mixed proxy list (HTTP + SOCKS + DIRECT).
TEST_F(MacSystemProxyResolverImplTest, MixedProxyList) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto http_dict =
      CreateProxyDictionary(kCFProxyTypeHTTP, "http-proxy.test", 8080);
  auto socks_dict =
      CreateProxyDictionary(kCFProxyTypeSOCKS, "socks-proxy.test", 1080);
  auto direct_dict = CreateProxyDictionaryForProxyTypeNone();
  mock->set_copy_proxies_for_url_result(
      CreateProxyArray({http_dict.get(), socks_dict.get(), direct_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kOk);
  EXPECT_FALSE(last_proxy_list().IsEmpty());
  EXPECT_EQ(last_proxy_list().ToDebugString(),
            "PROXY http-proxy.test:8080;SOCKS5 socks-proxy.test:1080;DIRECT");
}

// Tests that an empty proxy array returns kEmptyProxyList.
TEST_F(MacSystemProxyResolverImplTest, EmptyProxyArray) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  // Create an empty array.
  mock->set_copy_proxies_for_url_result(
      base::apple::ScopedCFTypeRef<CFArrayRef>(CFArrayCreate(
          kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks)));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kEmptyProxyList);
  EXPECT_TRUE(last_proxy_list().IsEmpty());
}

// Tests that PAC URL entries return kCFNetworkExecutePacScriptFailed.
TEST_F(MacSystemProxyResolverImplTest, PacUrlReturnsError) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto pac_dict =
      CreatePacUrlProxyDictionary("http://wpad.example.com/proxy.pac");
  mock->set_copy_proxies_for_url_result(CreateProxyArray({pac_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kCFNetworkExecutePacScriptFailed);
  EXPECT_TRUE(last_proxy_list().IsEmpty());
}

// Tests that inline PAC JavaScript entries return
// kCFNetworkExecutePacScriptFailed.
TEST_F(MacSystemProxyResolverImplTest, PacJavaScriptReturnsError) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto pac_dict = CreatePacJavaScriptProxyDictionary(
      "function FindProxyForURL(url, host) { return \"DIRECT\"; }");
  mock->set_copy_proxies_for_url_result(CreateProxyArray({pac_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_FALSE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kCFNetworkExecutePacScriptFailed);
  EXPECT_TRUE(last_proxy_list().IsEmpty());
}

// Tests that HTTPS proxy type (proxy applies to HTTPS URLs) works.
TEST_F(MacSystemProxyResolverImplTest, HttpsProxyType) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto proxy_dict =
      CreateProxyDictionary(kCFProxyTypeHTTPS, "secure-proxy.test", 443);
  mock->set_copy_proxies_for_url_result(CreateProxyArray({proxy_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kOk);
  EXPECT_FALSE(last_proxy_list().IsEmpty());
  // kCFProxyTypeHTTPS still maps to SCHEME_HTTP (proxy protocol is HTTP).
  EXPECT_EQ(last_proxy_list().ToDebugString(), "PROXY secure-proxy.test:443");
}

// Tests that a ws:// URL is handled successfully. Note: WebSocket scheme
// rewriting (ws:// -> http://) happens in MacAPIWrapperImpl::CopyProxiesForURL,
// not in the resolver itself. Since we use MockMacAPIWrapper, this test
// verifies the resolver passes the URL through and handles the result
// correctly.
TEST_F(MacSystemProxyResolverImplTest, WebSocketUrlProxyResolution) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto proxy_dict = CreateProxyDictionary(kCFProxyTypeHTTP, "proxy.test", 8080);
  mock->set_copy_proxies_for_url_result(CreateProxyArray({proxy_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("ws://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kOk);
  EXPECT_FALSE(last_proxy_list().IsEmpty());
  EXPECT_EQ(last_proxy_list().ToDebugString(), "PROXY proxy.test:8080");
}

// Tests that a wss:// URL is handled successfully.
TEST_F(MacSystemProxyResolverImplTest, SecureWebSocketUrlProxyResolution) {
  auto mock = std::make_unique<MockMacAPIWrapper>();
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto proxy_dict = CreateProxyDictionary(kCFProxyTypeHTTP, "proxy.test", 8080);
  mock->set_copy_proxies_for_url_result(CreateProxyArray({proxy_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("wss://example.com"));

  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
  EXPECT_EQ(last_status()->mac_proxy_status,
            net::MacProxyResolutionStatus::kOk);
  EXPECT_FALSE(last_proxy_list().IsEmpty());
  EXPECT_EQ(last_proxy_list().ToDebugString(), "PROXY proxy.test:8080");
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
  mock->set_copy_proxies_result(CreateEmptyProxySettings());

  auto proxy_dict = CreateProxyDictionary(kCFProxyTypeHTTP, "proxy.test", 8080);
  mock->set_copy_proxies_for_url_result(CreateProxyArray({proxy_dict.get()}));
  CreateResolver(std::move(mock));

  GetProxyForUrl(GURL("https://example1.com"));
  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);

  GetProxyForUrl(GURL("https://example2.com"));
  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);

  GetProxyForUrl(GURL("https://example3.com"));
  ASSERT_TRUE(last_status());
  EXPECT_TRUE(last_status()->is_success);
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
