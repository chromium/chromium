// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/resource_load_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/ip_address_space_overrides_test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::net::test_server::METHOD_GET;
using ::net::test_server::METHOD_OPTIONS;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

// These domains are mapped to the IP addresses above using the
// `--host-resolver-rules` command-line switch. The exact values come from the
// embedded HTTPS server, which has certificates for these domains
constexpr char kLoopbackHost[] = "a.test";
constexpr char kOtherLoopbackHost[] = "d.test";
// not localhost, but a host with IP address space = kLocal
constexpr char kLocalHost[] = "b.test";
constexpr char kPublicHost[] = "c.test";

// Path to a default response served by all servers in this test.
constexpr char kDefaultPath[] = "/defaultresponse";

// Path to a response with the `treat-as-public-address` CSP directive.
constexpr char kTreatAsPublicAddressPath[] =
    "/set-header?Content-Security-Policy: treat-as-public-address";

// Path to a response with a wide-open CORS header. This can be fetched
// cross-origin without triggering CORS violations.
constexpr char kCorsPath[] = "/set-header?Access-Control-Allow-Origin: *";

// Path to a cacheable response.
constexpr char kCacheablePath[] = "/cachetime";

// Path to a cacheable variant of `kCorsPath`.
constexpr char kCacheableCorsPath[] =
    "/set-header"
    "?Cache-Control: max-age%3D60"
    "&Access-Control-Allow-Origin: *";

// Returns a snippet of Javascript that fetch()es the given URL.
//
// The snippet evaluates to a boolean promise which resolves to true iff the
// fetch was successful. The promise never rejects, as doing so makes it hard
// to assert failure.
std::string FetchSubresourceScript(const GURL& url) {
  return JsReplace(
      R"(fetch($1).then(
           response => response.ok,
           error => {
             console.log('Error fetching ' + $1, error);
             return false;
           });
      )",
      url);
}

// A |ContentBrowserClient| implementation that allows modifying the return
// value of |ShouldAllowInsecurePrivateNetworkRequests()| at will.
class PolicyTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  PolicyTestContentBrowserClient() = default;

  PolicyTestContentBrowserClient(const PolicyTestContentBrowserClient&) =
      delete;
  PolicyTestContentBrowserClient& operator=(
      const PolicyTestContentBrowserClient&) = delete;

  ~PolicyTestContentBrowserClient() override = default;

  // Adds an origin to the allowlist.
  void SetAllowInsecurePrivateNetworkRequestsFrom(const url::Origin& origin) {
    allowlisted_origins_.insert(origin);
  }

  void SetWarnInsteadOfBlock() { warn_instead_of_block_ = true; }

  ContentBrowserClient::PrivateNetworkRequestPolicyOverride
  ShouldOverridePrivateNetworkRequestPolicy(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override {
    if (warn_instead_of_block_) {
      return ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
          kWarnInsteadOfBlock;
    }
    return allowlisted_origins_.find(origin) != allowlisted_origins_.end()
               ? ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
                     kForceAllow
               : ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
                     kDefault;
  }

 private:
  bool warn_instead_of_block_ = false;
  std::set<url::Origin> allowlisted_origins_;
};

// An embedded test server connection listener that simply counts connections.
// Thread-safe.
class ConnectionCounter
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  ConnectionCounter() = default;

  // Instances of this class are neither copyable nor movable.
  ConnectionCounter(const ConnectionCounter&) = delete;
  ConnectionCounter& operator=(const ConnectionCounter&) = delete;
  ConnectionCounter(ConnectionCounter&&) = delete;
  ConnectionCounter& operator=(ConnectionCounter&&) = delete;

  // Returns the number of sockets accepted by the servers we are listening to.
  int count() const {
    base::AutoLock guard(lock_);
    return count_;
  }

 private:
  // EmbeddedTestServerConnectionListener implementation.

  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> socket) override {
    {
      base::AutoLock guard(lock_);
      count_++;
    }
    return socket;
  }

  void ReadFromSocket(const net::StreamSocket& socket, int rv) override {}

  // `count_` is incremented on the embedded test server thread and read on the
  // test thread, so we synchronize accesses with a lock.
  mutable base::Lock lock_;
  int count_ GUARDED_BY(lock_) = 0;
};

class RequestObserver {
 public:
  RequestObserver() = default;

  // The returned callback must not outlive this instance.
  net::test_server::EmbeddedTestServer::MonitorRequestCallback BindCallback() {
    return base::BindRepeating(&RequestObserver::Observe,
                               base::Unretained(this));
  }

  // The origin of the URL is not checked for equality.
  std::vector<net::test_server::HttpMethod> RequestMethodsForUrl(
      const GURL& url) const {
    std::string path = url.PathForRequest();
    std::vector<net::test_server::HttpMethod> methods;
    {
      base::AutoLock guard(lock_);
      for (const auto& request : requests_) {
        if (request.GetURL().PathForRequest() == path) {
          methods.push_back(request.method);
        }
      }
    }
    return methods;
  }

 private:
  void Observe(const net::test_server::HttpRequest& request) {
    base::AutoLock guard(lock_);
    requests_.push_back(request);
  }

  // `requests_` is mutated on the embedded test server thread and read on the
  // test thread, so we synchronize accesses with a lock.
  mutable base::Lock lock_;
  std::vector<net::test_server::HttpRequest> requests_ GUARDED_BY(lock_);
};

// Removes `prefix` from the start of `str`, if present.
// Returns nullopt otherwise.
std::optional<std::string_view> StripPrefix(std::string_view str,
                                            std::string_view prefix) {
  if (!base::StartsWith(str, prefix)) {
    return std::nullopt;
  }

  return str.substr(prefix.size());
}

// Returns a pointer to the value of the `header` header in `request`, if any.
// Returns nullptr otherwise.
const std::string* FindRequestHeader(
    const net::test_server::HttpRequest& request,
    std::string_view header) {
  const auto it = request.headers.find(header);
  if (it == request.headers.end()) {
    return nullptr;
  }

  return &it->second;
}

// Returns the `Content-Range` header value for a given `range` of bytes out of
// the given `total_size` number of bytes.
std::string GetContentRangeHeader(const net::HttpByteRange& range,
                                  size_t total_size) {
  std::string first = base::NumberToString(range.first_byte_position());
  std::string last = base::NumberToString(range.last_byte_position());
  std::string total = base::NumberToString(total_size);
  return base::StrCat({"bytes ", first, "-", last, "/", total});
}

// An `EmbeddedTestServer` request handler function.
//
// Knows how to respond to CORS and PNA preflight requests, as well as regular
// and range requests.
//
// Route: /echorange?<body>
std::unique_ptr<net::test_server::HttpResponse> HandleRangeRequest(
    const net::test_server::HttpRequest& request) {
  std::optional<std::string_view> query =
      StripPrefix(request.relative_url, "/echorange?");
  if (!query) {
    return nullptr;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();

  constexpr std::pair<std::string_view, std::string_view> kCopiedHeaders[] = {
      {"Origin", "Access-Control-Allow-Origin"},
      {"Access-Control-Request-Private-Network",
       "Access-Control-Allow-Private-Network"},
      {"Access-Control-Request-Headers", "Access-Control-Allow-Headers"},
  };
  for (const auto& pair : kCopiedHeaders) {
    const std::string* value = FindRequestHeader(request, pair.first);
    if (value) {
      response->AddCustomHeader(pair.second, *value);
    }
  }

  // No body for a preflight response.
  if (request.method == net::test_server::METHOD_OPTIONS) {
    response->AddCustomHeader("Access-Control-Max-Age", "60");
    return response;
  }

  // Cache-Control: max-age=X does not work for range request caching. Use a
  // strong ETag instead, along with a last modified date. Both are required.
  response->AddCustomHeader("ETag", "foo");
  response->AddCustomHeader("Last-Modified", "Fri, 1 Apr 2022 12:34:56 UTC");

  const std::string* range_header = FindRequestHeader(request, "Range");
  if (!range_header) {
    // Not a range request. Respond with 200 and the whole query as the body.
    response->set_content(*query);
    return response;
  }

  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRangeHeader(*range_header, &ranges) ||
      ranges.size() != 1) {
    response->set_code(net::HTTP_BAD_REQUEST);
    return response;
  }

  net::HttpByteRange& range = ranges[0];
  if (!range.ComputeBounds(query->size())) {
    response->set_code(net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE);
    return response;
  }

  response->set_code(net::HTTP_PARTIAL_CONTENT);
  response->AddCustomHeader("Content-Range",
                            GetContentRangeHeader(range, query->size()));
  response->set_content(query->substr(range.first_byte_position(),
                                      range.last_byte_position() + 1));
  return response;
}

// A `net::EmbeddedTestServer` that pretends to be in a given IP address space.
//
// Set up of the command line in order for this server to be considered a part
// of `ip_address_space` must be done outside of server creation.
class FakeAddressSpaceServer {
 public:
  FakeAddressSpaceServer(net::EmbeddedTestServer::Type type,
                         net::test_server::HttpConnection::Protocol protocol,
                         network::mojom::IPAddressSpace ip_address_space,
                         const base::FilePath& test_data_path)
      : server_(type, protocol), ip_address_space_(ip_address_space) {
    // Use a certificate valid for multiple domains, which we can use to
    // distinguish `loopback`, `local` and `public` address spaces.
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    server_.SetConnectionListener(&connection_counter_);
    server_.RegisterRequestMonitor(request_observer_.BindCallback());
    server_.RegisterRequestHandler(base::BindRepeating(&HandleRangeRequest));
    server_.AddDefaultHandlers(test_data_path);
    CHECK(server_.Start());
  }

  std::string GenerateCommandLineSwitchOverride() const {
    return network::GenerateIpAddressSpaceOverride(server_, ip_address_space_);
  }

  // Returns the underlying test server.
  net::EmbeddedTestServer& Get() { return server_; }

  // Returns the total number of sockets accepted by this server.
  int ConnectionCount() const { return connection_counter_.count(); }

  const RequestObserver& request_observer() const { return request_observer_; }

 private:
  ConnectionCounter connection_counter_;
  RequestObserver request_observer_;
  net::EmbeddedTestServer server_;
  const network::mojom::IPAddressSpace ip_address_space_;
};

}  // namespace

// This being an integration/browser test, we concentrate on a few behaviors
// relevant to Local Network Access:
//
//  - testing the values of important properties on top-level documents:
//    - address space
//    - secure context bit
//    - private network request policy
//  - testing the inheritance semantics of these properties
//  - testing the correct handling of the CSP: treat-as-public-address directive
//  - testing that subresource requests are subject to LNA checks
//  - and a few other odds and ends
//
// We use the `--ip-address-space-overrides` command-line switch to test against
// `local` and `public` address spaces, even though all responses are actually
// served from localhost. Combined with host resolver rules, this lets us define
// three different domains that map to the different address spaces:
//
//  - `a.test` is `loopback`
//  - `b.test` is `local`
//  - `c.test` is `public`
//
// We also have unit tests that test all possible combinations of source and
// destination IP address spaces in services/network/url_loader_unittest.cc.
class LocalNetworkAccessBrowserTestBase : public ContentBrowserTest {
 public:
  RenderFrameHostImpl* root_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

 protected:
  // Allows subclasses to construct instances with different features enabled.
  explicit LocalNetworkAccessBrowserTestBase()
      : insecure_loopback_server_(
            net::EmbeddedTestServer::TYPE_HTTP,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kLoopback,
            GetTestDataFilePath()),
        insecure_local_server_(
            net::EmbeddedTestServer::TYPE_HTTP,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kLocal,
            GetTestDataFilePath()),
        insecure_public_server_(
            net::EmbeddedTestServer::TYPE_HTTP,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kPublic,
            GetTestDataFilePath()),
        secure_loopback_server_(
            net::EmbeddedTestServer::TYPE_HTTPS,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kLoopback,
            GetTestDataFilePath()),
        secure_local_server_(net::EmbeddedTestServer::TYPE_HTTPS,
                             net::test_server::HttpConnection::Protocol::kHttp1,
                             network::mojom::IPAddressSpace::kLocal,
                             GetTestDataFilePath()),
        secure_public_server_(
            net::EmbeddedTestServer::TYPE_HTTPS,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kPublic,
            GetTestDataFilePath()) {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Rules must be added on the main thread, otherwise `AddRule()` segfaults.
    host_resolver()->AddRule(kLoopbackHost, "127.0.0.1");
    host_resolver()->AddRule(kOtherLoopbackHost, "127.0.0.1");
    host_resolver()->AddRule(kLocalHost, "127.0.0.1");
    host_resolver()->AddRule(kPublicHost, "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // Add correct ip address space overrides.
    network::AddIpAddressSpaceOverridesToCommandLine(
        {insecure_loopback_server_.GenerateCommandLineSwitchOverride(),
         insecure_local_server_.GenerateCommandLineSwitchOverride(),
         insecure_public_server_.GenerateCommandLineSwitchOverride(),
         secure_loopback_server_.GenerateCommandLineSwitchOverride(),
         secure_local_server_.GenerateCommandLineSwitchOverride(),
         secure_public_server_.GenerateCommandLineSwitchOverride()},
        *command_line);
  }

  const FakeAddressSpaceServer& InsecureLoopbackServer() const {
    return insecure_loopback_server_;
  }

  const FakeAddressSpaceServer& InsecureLocalServer() const {
    return insecure_local_server_;
  }

  const FakeAddressSpaceServer& InsecurePublicServer() const {
    return insecure_public_server_;
  }

  const FakeAddressSpaceServer& SecureLoopbackServer() const {
    return secure_loopback_server_;
  }

  const FakeAddressSpaceServer& SecureLocalServer() const {
    return secure_local_server_;
  }

  const FakeAddressSpaceServer& SecurePublicServer() const {
    return secure_public_server_;
  }

  GURL InsecureLoopbackURL(const std::string& path) {
    return insecure_loopback_server_.Get().GetURL(kLoopbackHost, path);
  }

  GURL InsecureLocalURL(const std::string& path) {
    return insecure_local_server_.Get().GetURL(kLocalHost, path);
  }

  GURL InsecurePublicURL(const std::string& path) {
    return insecure_public_server_.Get().GetURL(kPublicHost, path);
  }

  GURL SecureLoopbackURL(const std::string& path) {
    return secure_loopback_server_.Get().GetURL(kLoopbackHost, path);
  }

  GURL OtherSecureLoopbackURL(const std::string& path) {
    return secure_loopback_server_.Get().GetURL(kOtherLoopbackHost, path);
  }

  GURL SecureLocalURL(const std::string& path) {
    return secure_local_server_.Get().GetURL(kLocalHost, path);
  }

  GURL SecurePublicURL(const std::string& path) {
    return secure_public_server_.Get().GetURL(kPublicHost, path);
  }

  GURL NullIPURL(const std::string& path) {
    return insecure_public_server_.Get().GetURL("0.0.0.0", path);
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  FakeAddressSpaceServer insecure_loopback_server_;
  FakeAddressSpaceServer insecure_local_server_;
  FakeAddressSpaceServer insecure_public_server_;
  FakeAddressSpaceServer secure_loopback_server_;
  FakeAddressSpaceServer secure_local_server_;
  FakeAddressSpaceServer secure_public_server_;
};

// Test with insecure local network subresource requests from the `public`
// address space blocked.
class LocalNetworkAccessBrowserTest : public LocalNetworkAccessBrowserTestBase {
 public:
  LocalNetworkAccessBrowserTest() : LocalNetworkAccessBrowserTestBase() {
    // Some builders run with field_trial disabled, need to enable this
    // manually.
    feature_list_.InitWithFeaturesAndParameters(
        {{network::features::kLocalNetworkAccessChecks,
          {{"LocalNetworkAccessChecksWarn", "false"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class LocalNetworkAccessBrowserTestDisableWebSecurity
    : public LocalNetworkAccessBrowserTest {
 public:
  LocalNetworkAccessBrowserTestDisableWebSecurity() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LocalNetworkAccessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }
};

// This is the same as LocalNetworkAccessBrowserTest, but runs each test twice,
// once with kOriginKeyedProcessesByDefault explicitly enabled, and once with it
// explicitly disabled. The tests implemented using this class involve sandboxed
// data: frames whose SiteInfo creation may vary depending on whether the
// feature is enabled or not.
class LocalNetworkAccessSandboxedDataBrowserTest
    : public LocalNetworkAccessBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalNetworkAccessSandboxedDataBrowserTest()
      : LocalNetworkAccessBrowserTestBase() {
    if (GetParam()) {
      feature_list_.InitWithFeaturesAndParameters(
          {{network::features::kLocalNetworkAccessChecks,
            {{"LocalNetworkAccessChecksWarn", "false"}}},
           {features::kOriginKeyedProcessesByDefault, {}}},
          {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          {
              {network::features::kLocalNetworkAccessChecks,
               {{"LocalNetworkAccessChecksWarn", "false"}}},
          },
          {features::kOriginKeyedProcessesByDefault});
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// ===========================
// CLIENT SECURITY STATE TESTS
// ===========================
//
// These tests verify the contents of `ClientSecurityState` for top-level
// documents in various different circumstances.

// This test checks the default security state.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, CheckSecurityState) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}

// This test verifies the contents of the ClientSecurityState for the initial
// empty document in a new main frame created by the browser.
//
// Note: the renderer-created main frame case is exercised by the
// OpeneeInherits* tests below.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForInitialEmptyDoc) {
  // Start a navigation. This forces the RenderFrameHost to initialize its
  // RenderFrame. The navigation is then cancelled by a HTTP 204 code.
  // We're left with a RenderFrameHost containing the default
  // ClientSecurityState values.
  //
  // Serve the response from a secure public server, to confirm that none of
  // the connection's properties are reflected in the committed document, which
  // is not a secure context and belongs to the `loopback` address space.
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), SecurePublicURL("/nocontent")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            security_state->cross_origin_embedder_policy.value);
  EXPECT_EQ(network::mojom::PrivateNetworkRequestPolicy::kBlock,
            security_state->private_network_request_policy);

  // Browser-created empty main frames are trusted to access the local network,
  // if they execute code injected via DevTools, WebView APIs or extensions.
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

// This test verifies the contents of the ClientSecurityState for `about:blank`
// in a new main frame created by the browser.
//
// Note: the renderer-created main frame case is exercised by the Openee
// inheritance tests below.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForAboutBlank) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForDataURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForFileURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "empty.html")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecureLoopbackAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecureLocalAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecurePublicAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSecureLoopbackAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSecureLocalAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSecurePublicAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Tests that a top-level navigation to 0.0.0.0 is in the kLoopback address
// space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForNullIP) {
  if constexpr (BUILDFLAG(IS_WIN)) {
    GTEST_SKIP() << "0.0.0.0 behavior varies across platforms and is "
                    "unreachable on Windows.";
  }

  EXPECT_TRUE(NavigateToURL(shell(), NullIPURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForTreatAsPublicAddress) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLoopbackURL(kTreatAsPublicAddressPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForTreatAsPublicAddressReportOnly) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      SecureLoopbackURL("/set-header?Content-Security-Policy-Report-Only: "
                        "treat-as-public-address")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForCachedSecureLoopbackDocument) {
  // Navigate to the cacheable document in order to cache it, then navigate
  // away.
  const GURL url = SecureLoopbackURL(kCacheablePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  // Navigate to the cached document.
  ResourceLoadObserver observer(shell());
  EXPECT_TRUE(NavigateToURL(shell(), url));
  observer.WaitForResourceCompletion(url);

  blink::mojom::ResourceLoadInfoPtr* info = observer.GetResource(url);
  ASSERT_TRUE(info);
  ASSERT_TRUE(*info);
  EXPECT_TRUE((*info)->was_cached);

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForCachedInsecurePublicDocument) {
  // Navigate to the cacheable document in order to cache it, then navigate
  // away.
  const GURL url = InsecurePublicURL(kCacheablePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  // Navigate to the cached document.
  ResourceLoadObserver observer(shell());
  EXPECT_TRUE(NavigateToURL(shell(), url));
  observer.WaitForResourceCompletion(url);

  blink::mojom::ResourceLoadInfoPtr* info = observer.GetResource(url);
  ASSERT_TRUE(info);
  ASSERT_TRUE(*info);
  EXPECT_TRUE((*info)->was_cached);

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// This test verifies that the chrome:// scheme is considered loopback for the
// purpose of Local Network Access.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeURL) {
  // Not all chrome:// hosts are available in content/ but ukm is one of them.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("chrome://ukm")));
  EXPECT_TRUE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeUIScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

// The view-source:// scheme should only ever appear in the display URL. It
// shouldn't affect the IPAddressSpace computation. This test verifies that we
// end up with the response IPAddressSpace.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeViewSourcePublic) {
  const GURL url = SecurePublicURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("view-source:" + url.spec())));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kViewSourceScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Variation of above test with a local address.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeViewSourceLocal) {
  const GURL url = SecureLocalURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("view-source:" + url.spec())));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kViewSourceScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// The chrome-error:// scheme should only ever appear in origins. It shouldn't
// affect the IPAddressSpace computation. This test verifies that we end up with
// the response IPAddressSpace. Error pages should not be considered secure
// contexts however.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeErrorPublic) {
  EXPECT_FALSE(NavigateToURL(shell(), SecurePublicURL("/empty404.html")));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Variation of above test with a local address.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeErrorLocal) {
  EXPECT_FALSE(NavigateToURL(shell(), SecureLocalURL("/empty404.html")));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// ========================
// INHERITANCE TEST HELPERS
// ========================

namespace {

// Creates a blob containing dummy HTML, then returns its URL.
// Executes javascript to do so in |frame_host|, which must not be nullptr.
GURL CreateBlobURL(RenderFrameHostImpl* frame_host) {
  // Define a variable to avoid awkward `ExtractString()` indentation.
  EvalJsResult result = EvalJs(frame_host, R"(
    const blob = new Blob(["foo"], {type: "text/html"});
    URL.createObjectURL(blob)
  )");
  return GURL(result.ExtractString());
}

// Executes |script| to add a new child iframe to the given |parent| document.
//
// |parent| must not be nullptr.
//
// Returns a pointer to the child frame host.
RenderFrameHostImpl* AddChildWithScript(RenderFrameHostImpl* parent,
                                        const std::string& script) {
  size_t initial_child_count = parent->child_count();

  EXPECT_EQ(true, ExecJs(parent, script));

  EXPECT_EQ(parent->child_count(), initial_child_count + 1);
  if (parent->child_count() < initial_child_count + 1) {
    return nullptr;
  }

  return parent->child_at(initial_child_count)->current_frame_host();
}

// Adds a child iframe sourced from `url` to the given `parent` document and
// waits for it to load. Returns the child RFHI.
//
// `parent` must not be nullptr.
RenderFrameHostImpl* AddChildFromURL(RenderFrameHostImpl* parent,
                                     std::string_view url) {
  constexpr std::string_view kScriptTemplate = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )";
  return AddChildWithScript(parent, JsReplace(kScriptTemplate, url));
}

// Convenience overload for absolute URLs.
RenderFrameHostImpl* AddChildFromURL(RenderFrameHostImpl* parent,
                                     const GURL& url) {
  return AddChildFromURL(parent, url.spec());
}

RenderFrameHostImpl* AddChildFromAboutBlank(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, "about:blank");
}

RenderFrameHostImpl* AddChildInitialEmptyDoc(RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/nocontent";  // Returns 204 NO CONTENT, thus no doc commits.
    document.body.appendChild(iframe);
    true  // Do not wait for iframe.onload, which never fires.
  )");
}

RenderFrameHostImpl* AddChildFromSrcdoc(RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.srcdoc = "foo";
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )");
}

RenderFrameHostImpl* AddChildFromDataURL(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, "data:text/html,foo");
}

RenderFrameHostImpl* AddChildFromJavascriptURL(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, "javascript:'foo'");
}

RenderFrameHostImpl* AddChildFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return AddChildFromURL(parent, blob_url);
}

RenderFrameHostImpl* AddSandboxedChildFromURL(RenderFrameHostImpl* parent,
                                              const GURL& url) {
  std::string script_template = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.sandbox = "";
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )";
  return AddChildWithScript(parent, JsReplace(script_template, url));
}

RenderFrameHostImpl* AddSandboxedChildFromAboutBlank(
    RenderFrameHostImpl* parent) {
  return AddSandboxedChildFromURL(parent, GURL("about:blank"));
}

RenderFrameHostImpl* AddSandboxedChildInitialEmptyDoc(
    RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/nocontent";  // Returns 204 NO CONTENT, thus no doc commits.
    iframe.sandbox = "";
    document.body.appendChild(iframe);
    true  // Do not wait for iframe.onload, which never fires.
  )");
}

RenderFrameHostImpl* AddSandboxedChildFromSrcdoc(RenderFrameHostImpl* parent) {
  return AddChildWithScript(parent, R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.srcdoc = "foo";
      iframe.sandbox = "";
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )");
}

RenderFrameHostImpl* AddSandboxedChildFromDataURL(RenderFrameHostImpl* parent) {
  return AddSandboxedChildFromURL(parent, GURL("data:text/html,foo"));
}

RenderFrameHostImpl* AddSandboxedChildFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return AddSandboxedChildFromURL(parent, blob_url);
}

// Returns the main frame RenderFrameHostImpl in the given |shell|.
//
// |shell| must not be nullptr.
//
// Helper for OpenWindow*().
RenderFrameHostImpl* GetPrimaryMainFrameHostImpl(Shell* shell) {
  return static_cast<RenderFrameHostImpl*>(
      shell->web_contents()->GetPrimaryMainFrame());
}

// Opens a new window from within |parent|, pointed at the given |url|.
// Waits until the openee window has navigated to |url|, then returns a pointer
// to its main frame RenderFrameHostImpl.
//
// |parent| must not be nullptr.
RenderFrameHostImpl* OpenWindowFromURL(RenderFrameHostImpl* parent,
                                       const GURL& url) {
  return GetPrimaryMainFrameHostImpl(OpenPopup(parent, url, "_blank"));
}

RenderFrameHostImpl* OpenWindowFromAboutBlank(RenderFrameHostImpl* parent) {
  return OpenWindowFromURL(parent, GURL("about:blank"));
}

// Same as above, but with the "noopener" window feature.
RenderFrameHostImpl* OpenWindowFromAboutBlankNoOpener(
    RenderFrameHostImpl* parent) {
  // Setting the "noopener" window feature makes `window.open()` return `null`.
  constexpr bool kNoExpectReturnFromWindowOpen = false;

  return GetPrimaryMainFrameHostImpl(OpenPopup(parent, GURL("about:blank"),
                                               "_blank", "noopener",
                                               kNoExpectReturnFromWindowOpen));
}

RenderFrameHostImpl* OpenWindowFromURLExpectNoCommit(
    RenderFrameHostImpl* parent,
    const GURL& url,
    std::string_view features = "") {
  ShellAddedObserver observer;

  std::string_view script_template = R"(
    window.open($1, "_blank", $2);
  )";
  EXPECT_TRUE(ExecJs(parent, JsReplace(script_template, url, features)));

  return GetPrimaryMainFrameHostImpl(observer.GetShell());
}

RenderFrameHostImpl* OpenWindowInitialEmptyDoc(RenderFrameHostImpl* parent) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.
  return OpenWindowFromURLExpectNoCommit(parent, GURL("/nocontent"));
}

// Same as above, but with the "noopener" window feature.
RenderFrameHostImpl* OpenWindowInitialEmptyDocNoOpener(
    RenderFrameHostImpl* parent) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.
  return OpenWindowFromURLExpectNoCommit(parent, GURL("/nocontent"),
                                         "noopener");
}

GURL JavascriptURL(std::string_view script) {
  return GURL(base::StrCat({"javascript:", script}));
}

RenderFrameHostImpl* OpenWindowFromJavascriptURL(
    RenderFrameHostImpl* parent,
    std::string_view script = "'foo'") {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation, since the `javascript:` URL will not commit (`about:blank`
  // will).
  return OpenWindowFromURLExpectNoCommit(parent, JavascriptURL(script));
}

// Same as above, but with the "noopener" window feature.
RenderFrameHostImpl* OpenWindowFromJavascriptURLNoOpener(
    RenderFrameHostImpl* parent,
    std::string_view script) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.
  return OpenWindowFromURLExpectNoCommit(parent, JavascriptURL(script),
                                         "noopener");
}

RenderFrameHostImpl* OpenWindowFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return OpenWindowFromURL(parent, blob_url);
}

RenderFrameHostImpl* GetFirstChild(RenderFrameHostImpl& parent) {
  CHECK_NE(parent.child_count(), 0ul);
  return parent.child_at(0)->current_frame_host();
}

// Adds a child iframe sourced from `url` to the given `parent` document.
// Does not wait for the child frame to load - this must be done separately.
//
// `parent` must not be nullptr.
void AddChildFromURLWithoutWaiting(RenderFrameHostImpl* parent,
                                   std::string_view url) {
  // Define a variable for better indentation.
  constexpr std::string_view kScriptTemplate = R"(
    const child = document.createElement("iframe");
    child.src = $1;
    document.body.appendChild(child);
  )";

  EXPECT_EQ(true, ExecJs(parent, JsReplace(kScriptTemplate, url)));
}

// Convenience overload for absolute URLs.
void AddChildFromURLWithoutWaiting(RenderFrameHostImpl* parent,
                                   const GURL& url) {
  return AddChildFromURLWithoutWaiting(parent, url.spec());
}

}  // namespace

// ===============================
// ADDRESS SPACE INHERITANCE TESTS
// ===============================
//
// These tests verify that `ClientSecurityState.ip_address_space` is correctly
// inherited by child iframes and openee documents for a variety of URLs with
// local schemes.

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`
// inherits its address space from the opener. In this case, the opener's
// address space is `public`.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`
// inherits its address space from the opener. In this case, the opener's
// address space is `loopback`.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`,
// opened with the "noopener" feature, has its address space set to `loopback`
// regardless of the address space of the opener.
//
// Compare and contrast against the above tests without "noopener".
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForAboutBlankIsLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window =
      OpenWindowFromAboutBlankNoOpener(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    IframeInheritsAddressSpaceForInitialEmptyDocFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document inherits its address space from the opener. In this case, the
// opener's address space is `public`.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document inherits its address space from the opener. In this case, the
// opener's address space is `loopback`.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    OpeneeInheritsAddressSpaceForInitialEmptyDocFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document, opened with the "noopener" feature, has its address space set to
// `loopback` regardless of the address space of the opener.
//
// Compare and contrast against the above tests without "noopener".
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForInitialEmptyDocIsLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window =
      OpenWindowInitialEmptyDocNoOpener(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_P(
    LocalNetworkAccessSandboxedDataBrowserTest,
    SandboxedIframeInheritsAddressSpaceForDataURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_P(
    LocalNetworkAccessSandboxedDataBrowserTest,
    SandboxedIframeInheritsAddressSpaceForDataURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(
      root_frame_host(), "var injectedCodeWasExecuted = true");
  ASSERT_NE(nullptr, window);

  // The Javascript in the URL got executed in the new window.
  EXPECT_EQ(true, EvalJs(window, "injectedCodeWasExecuted"));

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForJavascriptURLIsLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURLNoOpener(
      root_frame_host(), "var injectedCodeWasExecuted = true");
  ASSERT_NE(nullptr, window);

  // The Javascript in the URL was not executed in the new window. This ensures
  // it is safe to classify the new window as `loopback` without allowing the
  // opener to execute arbitrary JS in the `loopback` address space.
  EXPECT_EQ("undefined", EvalJs(window, "typeof injectedCodeWasExecuted"));

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForBlobURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromLoopback) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLoopback,
            security_state->ip_address_space);
}

INSTANTIATE_TEST_SUITE_P(,
                         LocalNetworkAccessSandboxedDataBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "OriginKeyedProcessesByDefault_disabled"
                                      : "OriginKeyedProcessesByDefault_enabled";
                         });

// ================================
// SECURE CONTEXT INHERITANCE TESTS
// ================================
//
// These tests verify that `ClientSecurityState.is_web_secure_context` is
// correctly inherited by child iframes and openee documents for a variety of
// URLs with local schemes.

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_P(
    LocalNetworkAccessSandboxedDataBrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    IframeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

// ====================================
// PRIVATE NETWORK REQUEST POLICY TESTS
// ====================================
//
// These tests verify the correct setting of
// `ClientSecurityState.private_network_request_policy` in various situations.

// If --disable-web-security is set, allow all LNA requests.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTestDisableWebSecurity,
                       LocalNetworkPolicyIsAllowInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTestDisableWebSecurity,
                       LocalNetworkPolicyIsAllowSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to block requests from non-secure
// contexts in the `public` address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkPolicyIsBlockForInsecurePublic) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to block requests from non-secure
// contexts in the `local` address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkPolicyIsBlockForInsecureLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to block requests from non-secure
// contexts in the `unknown` address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkPolicyIsBlockForInsecureUnknown) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to ask for permission from secure
// contexts in the `public` address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkPolicyIsPermissionBlockForSecurePublic) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to ask for permission from secure
// contexts in the `local` address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkPolicyIsPermissionBlockForSecureLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}

// This test verifies that the initial empty document, which inherits its origin
// from the document creator, also inherits its private network request policy.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    LocalNetworkRequestPolicyInheritedWithOriginForInitialEmptyDoc) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that `about:blank` iframes, which inherit their origin
// from the navigation initiator, also inherit their private network request
// policy.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    LocalNetworkRequestPolicyInheritedWithOriginForAboutBlank) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that `data:` iframes, which commit an opaque origin
// derived from the navigation initiator's origin, do not inherit their private
// network request policy.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    LocalNetworkRequestPolicyNotInheritedWithOriginForDataURL) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that sandboxed iframes, which commit an opaque origin
// derived from the navigation initiator's origin, do not inherit their private
// network request policy.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    LocalNetworkRequestPolicyNotInheritedForSandboxedInitialEmptyDoc) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that sandboxed iframes, which commit an opaque origin
// derived from the navigation initiator's origin, do not inherit their private
// network request policy. "about:blank" behaves slightly differently from the
// initial empty doc in code, but should have the same policy in the end.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    LocalNetworkRequestPolicyNotInheritedForSandboxedAboutBlank) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that error pages have a set private network request
// policy of `kBlock` irrespective of the navigation initiator.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkRequestPolicyIsBlockForErrorPage) {
  GURL url = InsecurePublicURL(kDefaultPath);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddChildFromURL(root_frame_host(), "/close-socket");

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that child frames with distinct origins from their parent
// do not inherit their private network request policy, which is based on the
// origin of the child document instead.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkRequestPolicyCalculatedPerOrigin) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddChildFromURL(root_frame_host(), InsecureLoopbackURL(kDefaultPath));

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

class LocalNetworkAccessBrowserTestWithWarnInsteadOfBlockOption
    : public LocalNetworkAccessBrowserTest,
      public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    LocalNetworkAccessBrowserTestWithWarnInsteadOfBlockOption,
    testing::Values(false, true));

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests can be overridden to warn instead of
// block for insecure contexts in the `public` address space.
IN_PROC_BROWSER_TEST_P(
    LocalNetworkAccessBrowserTestWithWarnInsteadOfBlockOption,
    LocalNetworkPolicyCanWarnForInsecurePublic) {
  PolicyTestContentBrowserClient client;
  bool warn_instead_of_block = GetParam();
  if (warn_instead_of_block) {
    client.SetWarnInsteadOfBlock();
  }

  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            warn_instead_of_block
                ? network::mojom::PrivateNetworkRequestPolicy::kWarn
                : network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests can be overridden to warn instead of
// block for secure contexts in the `public` address space.
IN_PROC_BROWSER_TEST_P(
    LocalNetworkAccessBrowserTestWithWarnInsteadOfBlockOption,
    LocalNetworkPolicyCanWarnForSecurePublic) {
  PolicyTestContentBrowserClient client;
  bool warn_instead_of_block = GetParam();
  if (warn_instead_of_block) {
    client.SetWarnInsteadOfBlock();
  }

  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(
      security_state->private_network_request_policy,
      warn_instead_of_block
          ? network::mojom::PrivateNetworkRequestPolicy::kPermissionWarn
          : network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}

// =======================
// SUBRESOURCE FETCH TESTS
// =======================
//
// These tests verify the behavior of the browser when fetching subresources
// across IP address spaces.

// Check that the `--disable-web-security` command-line switch disables LNA
// checks.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTestDisableWebSecurity,
                       LocalNetworkRequestIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  // Check that the page can load a loopback resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from a secure page served from a public IP address
//  - to a loopback IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromSecurePublicToLoopbackNoPermissionIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(SecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from a secure page served from a local IP address
//  - to a loopback IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromSecureLocalToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from a secure page served from a loopback IP address
//  - to a loopback IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromSecureLoopbackToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  // Check that the page can load a loopback resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(OtherSecureLoopbackURL(kCorsPath))));
}

// This test verifies that when the content browser client overrides it,
// requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a loopback IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    FromInsecureTreatAsPublicToLoopbackWithPolicySetToAllowIsNotBlocked) {
  GURL url = InsecureLoopbackURL(kTreatAsPublicAddressPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);

  // Check that the page can load a loopback resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a loopback IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromInsecureTreatAsPublicToLoopbackIsBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLoopbackURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load a loopback resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from an insecure page served by a public IP address
//  - to loopback IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromInsecurePublicToLoopbackIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  // Check that the page cannot load a loopback resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from an insecure page served by a local IP address
//  - to loopback IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromInsecureLocalToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page can load a loopback resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from an insecure page served by a loopback IP address
//  - to loopback IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromInsecureLoopbackToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  // Check that the page can load a loopback resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - embedded in an insecure page served from a loopback IP address
//  - to loopback IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    FromSecurePublicEmbeddedInInsecureLoopbackToLoopbackIsBlocked) {
  // First navigate to an insecure page served by a loopback IP address.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  // Then embed a secure public iframe.
  std::string script = JsReplace(
      R"(
        const iframe = document.createElement("iframe");
        iframe.src = $1;
        document.body.appendChild(iframe);
      )",
      SecurePublicURL(kDefaultPath));
  EXPECT_TRUE(ExecJs(root_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  // Even though the iframe document was loaded from a secure connection, the
  // context is deemed insecure because it was embedded by an insecure context.
  EXPECT_FALSE(security_state->is_web_secure_context);

  // The address space of the document, however, is not influenced by the
  // parent's address space.
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the iframe cannot load a loopback resource.
  EXPECT_EQ(false, EvalJs(child_frame, FetchSubresourceScript(
                                           InsecureLoopbackURL(kCorsPath))));
}

// This test verifies that requests:
//  - from a non-secure context in the `loopback` IP address space
//  - to a subresource cached from a `loopback` IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromInsecureLoopbackToCachedLoopbackIsNotBlocked) {
  GURL target = InsecureLoopbackURL(kCacheablePath);

  // Cache the resource first. The server receives a GET request.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  // Check that the page can still load the subresource from cache. The server
  // does not receive any new request.
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));
}

// This test verifies that requests:
//  - from a non-secure context in the `public` IP address space
//  - to a subresource cached from a `loopback` IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromInsecurePublicToCachedLoopbackIsBlocked) {
  GURL target = InsecureLoopbackURL(kCacheablePath);

  // Cache the resource first, by fetching it from a document in the same IP
  // address space. The server receives a GET request.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  // Now navigate to a document in the `public` address space belonging to the
  // same site as the previous document (this will use the same cache key).
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLoopbackURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load the resource, even from cache. The server
  // does not receive any new request.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));
}

// This test verifies that requests:
//  - from a secure context in the `loopback` IP address space
//  - to a subresource cached from a `loopback` IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromSecureLoopbackToCachedLoopbackIsNotBlocked) {
  GURL target = SecureLoopbackURL(kCacheablePath);

  // Cache the resource first. The server receives a GET request.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  // Check that the page can still load the subresource from cache. The server
  // does not receive any new request.
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));
}

// This test verifies that requests:
//  - from a secure page served in the `public` IP address space
//  - to a subresource cached from a `loopback` IP address
//  are blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FromSecurePublicToCachedLoopbackIsBlocked) {
  GURL target = OtherSecureLoopbackURL(kCacheableCorsPath);

  // Cache the resource first.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLoopbackURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load the subresource from cache.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
}

// This test verifies that an insecure page in the `loopback` address space
// cannot fetch a `file:` URL.
//
// This is relevant to Local Network Access, since `file:` URLs are considered
// to be in the `loopback` IP address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       InsecurePageCannotRequestFile) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(GetTestUrl(
                                                 "", "empty.html"))));
}

// This test verifies that a secure page in the `loopback` address space cannot
// fetch a `file:` URL.
//
// This is relevant to Local Network Access, since `file:` URLs are considered
// to be in the `loopback` IP address space.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       SecurePageCannotRequestFile) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLoopbackURL(kDefaultPath)));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(GetTestUrl(
                                                 "", "empty.html"))));
}

// This test verifies that if a page redirects after responding to a local
// network request to a server in a different address space, the request does
// not fail.
// Regression test for https://crbug.com/1293891.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, Redirect) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  GURL target =
      SecureLoopbackURL("/server-redirect?" + SecureLocalURL(kCorsPath).spec());

  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
}

// =========================
// WORKER SCRIPT FETCH TESTS
// =========================

namespace {

// Path to a worker script that posts a message to its creator once loaded.
constexpr char kWorkerScriptPath[] = "/workers/post_ready.js";

// Instantiates a dedicated worker script from `path`.
// If it loads successfully, the worker should post a message to its creator to
// signal success.
std::string FetchWorkerScript(std::string_view path) {
  constexpr char kTemplate[] = R"(
    new Promise((resolve) => {
      const worker = new Worker($1);
      worker.addEventListener("message", () => resolve(true));
      worker.addEventListener("error", () => resolve(false));
    })
  )";

  return JsReplace(kTemplate, path);
}

// Path to a worker script that posts a message to each client that connects.
constexpr char kSharedWorkerScriptPath[] = "/workers/shared_post_ready.js";

// Instantiates a shared worker script from `path`.
// If it loads successfully, the worker should post a message to each client
// that connects to it to signal success.
std::string FetchSharedWorkerScript(std::string_view path) {
  constexpr char kTemplate[] = R"(
    new Promise((resolve) => {
      const worker = new SharedWorker($1);
      worker.port.addEventListener("message", () => resolve(true));
      worker.addEventListener("error", () => resolve(false));
      worker.port.start();
    })
  )";

  return JsReplace(kTemplate, path);
}

// TODO(crbug.com/40290702): Remove this and replace calls below with
// calls to `EXPECT_EQ` directly once Shared Workers are supported on Android.
void ExpectFetchSharedWorkerScriptResult(bool expected,
                                         const EvalJsResult& result) {
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(expected, result);
#else
  EXPECT_FALSE(result.is_ok());
#endif
}

}  // namespace

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FetchWorkerFromInsecureTreatAsPublicToLoopback) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLoopbackURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FetchWorkerFromSecureTreatAsPublicToLoopback) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLoopbackURL(kTreatAsPublicAddressPath)));

  // The request is exempt from Local Network Access checks because it is
  // same-origin and the origin is potentially trustworthy. Dedicated worker
  // scripts are required to be same-origin.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FetchSharedWorkerFromInsecureTreatAsPublicToLoopback) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLoopbackURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      false, EvalJs(root_frame_host(),
                    FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       FetchSharedWorkerFromSecureTreatAsPublicToLoopback) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLoopbackURL(kTreatAsPublicAddressPath)));

  // The request is exempt from Local Network Access checks because it is
  // same-origin and the origin is potentially trustworthy. Shared worker
  // scripts are required to be same-origin.
  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

// ======================
// NAVIGATION FETCH TESTS
// ======================
//
// These tests verify the behavior of the browser when navigating across IP
// address spaces.
//
// Iframe navigations are effectively treated as subresource fetches of the
// initiator document: they are handled by checking the resource's address space
// against the initiator document's address space.
//
// Top-level navigations are never blocked.
//
// TODO(crbug.com/40263397): Revisit this when top-level navigations are
// subject to Private Network Access checks.

// This test verifies that  iframe requests:
//  - from an insecure page served from a public IP address
//  - to a loopback IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeFromInsecurePublicToLoopbackIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));
  GURL url = InsecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());

  RenderFrameHostImpl* child_frame = GetFirstChild(*root_frame_host());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The frame committed an error page but retains the original URL so that
  // reloading the page does the right thing. The committed origin on the other
  // hand is opaque, which it would not be if the navigation had succeeded.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());

  // Blocked before we ever sent a request.
  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      IsEmpty());
}

// Same as above, testing the "treat-as-public-address" CSP directive.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeFromInsecureTreatAsPublicToLoopbackIsBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLoopbackURL(kTreatAsPublicAddressPath)));

  GURL url = InsecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());
}

// This test verifies that when an iframe navigation fails due to LNA, the
// iframe navigates to an error page, even if it had previously committed a
// document.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeFailedNavigationCommitsErrorPage) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  // First add a child frame, which successfully commits a document.
  AddChildFromURL(root_frame_host(), "/empty.html");

  GURL url = InsecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  // Then try to navigate that frame in a way that fails LNA checks.
  EXPECT_TRUE(ExecJs(
      root_frame_host(),
      JsReplace("document.getElementsByTagName('iframe')[0].src = $1;", url)));
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());

  RenderFrameHostImpl* child_frame = GetFirstChild(*root_frame_host());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The frame committed an error page but retains the original URL so that
  // reloading the page does the right thing. The committed origin on the other
  // hand is opaque, which it would not be if the navigation had succeeded.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());

  // Blocked before we ever sent a request.
  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      IsEmpty());
}

// This test verifies that iframe requests:
//  - from a secure page served from a public IP address
//  - to a loopback IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeFromSecurePublicToLoopbackIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());

  RenderFrameHostImpl* child_frame = GetFirstChild(*root_frame_host());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The frame committed an error page but retains the original URL so that
  // reloading the page does the right thing. The committed origin on the other
  // hand is opaque, which it would not be if the navigation had succeeded.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());

  // Blocked before we ever sent a request.
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      IsEmpty());
}

// Same as above, testing the "treat-as-public-address" CSP directive.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       IframeFromSecureTreatAsPublicToLoopbackIsNotBlocked) {
  GURL initiator_url = SecureLoopbackURL(kTreatAsPublicAddressPath);
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  GURL url = OtherSecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());

  RenderFrameHostImpl* child_frame = GetFirstChild(*root_frame_host());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The frame committed an error page but retains the original URL so that
  // reloading the page does the right thing. The committed origin on the other
  // hand is opaque, which it would not be if the navigation had succeeded.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());

  // Blocked before we ever sent a request.
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      IsEmpty());
}

// Form submissions from the main frame are not blocked as we do not block main
// frame navigations.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    FormSubmissionFromInsecurePublicToLoopbackIsNotBlockedInMainFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLoopbackURL(kDefaultPath);
  TestNavigationManager navigation_manager(shell()->web_contents(), url);

  std::string_view script_template = R"(
    const form = document.createElement("form");
    form.action = $1;
    form.method = "post";
    document.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(ExecJs(root_frame_host(), JsReplace(script_template, url)));

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Check that the form submission was not blocked.
  EXPECT_TRUE(navigation_manager.was_successful());
}

// Form submissions
//  - from a secure page served from a public IP address
//  - to a loopback IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    FormSubmissionFromSecurePublicToLoopbackIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLoopbackURL(kDefaultPath);
  TestNavigationManager navigation_manager(shell()->web_contents(), url);

  std::string_view script_template = R"(
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);

    const childDoc = iframe.contentDocument;
    const form = childDoc.createElement("form");
    form.action = $1;
    form.method = "post";
    childDoc.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(ExecJs(root_frame_host(), JsReplace(script_template, url)));

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe was blocked.
  EXPECT_FALSE(navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();

  // Failed navigation.
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The URL is the form target URL, to allow for reloading.
  // The origin is opaque though, a symptom of the failed navigation.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

// Form submissions
//  - from a secure page served from a public IP address
//  - to a loopback IP address
//  - using a GET method
// are blocked.
IN_PROC_BROWSER_TEST_F(
    LocalNetworkAccessBrowserTest,
    FormSubmissionGetFromSecurePublicToLoopbackIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL target_url = SecureLoopbackURL(kDefaultPath);

  // The page navigates to `url` followed by an empty query: '?'.
  GURL expected_url = GURL(target_url.spec() + "?");
  TestNavigationManager navigation_manager(shell()->web_contents(),
                                           expected_url);

  std::string_view script_template = R"(
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);

    const childDoc = iframe.contentDocument;
    const form = childDoc.createElement("form");
    form.action = $1;
    form.method = "get";
    childDoc.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(
      ExecJs(root_frame_host(), JsReplace(script_template, target_url)));

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe was blocked.
  EXPECT_FALSE(navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();

  // Failed navigation.
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The URL is the form target URL, to allow for reloading.
  // The origin is opaque though, a symptom of the failed navigation.
  EXPECT_EQ(expected_url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       SiblingNavigationFromInsecurePublicToLoopbackIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLoopbackURL(kDefaultPath)));

  // Named targeting only works if the initiator is one of:
  //
  //  - the target's parent -> uninteresting
  //  - the target's opener -> implies the target is a main frame
  //  - same-origin with the target -> the only option left
  //
  // Thus we use CSP: treat-as-public-address to place the initiator in a
  // different IP address space as its same-origin target.
  GURL initiator_url = InsecureLoopbackURL(kTreatAsPublicAddressPath);
  GURL target_url = InsecureLoopbackURL(kDefaultPath);

  constexpr std::string_view kScriptTemplate = R"(
    function addChild(name, src) {
      return new Promise((resolve) => {
        const iframe = document.createElement("iframe");
        iframe.name = name;
        iframe.src = src;
        iframe.onload = () => resolve(iframe);
        document.body.appendChild(iframe);
      });
    }

    Promise.all([
      addChild("initiator", $1),
      addChild("target", "/empty.html"),
    ]).then(() => true);
  )";

  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         JsReplace(kScriptTemplate, initiator_url)));

  ASSERT_EQ(2ul, root_frame_host()->child_count());
  RenderFrameHostImpl* initiator =
      root_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(initiator->GetLastCommittedURL(), initiator_url);

  TestNavigationManager navigation_manager(shell()->web_contents(), target_url);

  EXPECT_TRUE(
      ExecJs(initiator, JsReplace("window.open($1, 'target')", target_url)));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe was blocked.
  EXPECT_FALSE(navigation_manager.was_successful());

  // Request was blocked before it was even sent.
  EXPECT_THAT(SecureLoopbackServer().request_observer().RequestMethodsForUrl(
                  target_url),
              IsEmpty());
}

}  // namespace content
