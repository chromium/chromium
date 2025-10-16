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

// Note: tests in this file are being migrated to work for Local Network Access;
// please do not add new tests to this file. Instead, tests should be added to
// content/browser/renderer_host/local_network_access_browsertest.cc

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

// Returns a path to a response that passes Private Network Access checks.
//
// This can be used to construct the `src` URL for an iframe.
std::string MakePnaPathForIframe(const url::Origin& initiator_origin) {
  return base::StrCat({
      "/set-header"
      // Apparently a wildcard `*` is not sufficient in this case, so we need
      // to explicitly allow the initiator origin instead.
      "?Access-Control-Allow-Origin: ",
      initiator_origin.Serialize(),
      "&Access-Control-Allow-Private-Network: true"
      // It seems navigation requests carry credentials...
      "&Access-Control-Allow-Credentials: true"
      // And the following couple headers.
      "&Access-Control-Allow-Headers: upgrade-insecure-requests,accept",
  });
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

  void SetBlockInsteadOfWarn() { block_instead_of_warn_ = true; }

  ContentBrowserClient::PrivateNetworkRequestPolicyOverride
  ShouldOverridePrivateNetworkRequestPolicy(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override {
    if (block_instead_of_warn_) {
      return ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
          kBlockInsteadOfWarn;
    }
    return allowlisted_origins_.find(origin) != allowlisted_origins_.end()
               ? ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
                     kForceAllow
               : ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
                     kDefault;
  }

 private:
  bool block_instead_of_warn_ = false;
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
// relevant to Private Network Access:
//
//  - testing the values of important properties on top-level documents:
//    - address space
//    - secure context bit
//    - private network request policy
//  - testing the inheritance semantics of these properties
//  - testing the correct handling of the CSP: treat-as-public-address directive
//  - testing that subresource requests are subject to PNA checks
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
class PrivateNetworkAccessBrowserTestBase : public ContentBrowserTest {
 public:
  RenderFrameHostImpl* root_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

 protected:
  // Allows subclasses to construct instances with different features enabled.
  explicit PrivateNetworkAccessBrowserTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features)
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
            GetTestDataFilePath()) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

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

// Test with insecure private network subresource requests from the `public`
// address space blocked and preflights otherwise enabled but not enforced.
class PrivateNetworkAccessBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessSendPreflights,
            },
            {
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

class PrivateNetworkAccessBrowserTestDisableWebSecurity
    : public PrivateNetworkAccessBrowserTest {
 public:
  PrivateNetworkAccessBrowserTestDisableWebSecurity() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrivateNetworkAccessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }
};

// Test with insecure private network subresource requests blocked, including
// from the `private` address space.
class PrivateNetworkAccessBrowserTestBlockFromPrivate
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestBlockFromPrivate()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kPrivateNetworkAccessRespectPreflightResults,
            },
            {
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with insecure private network subresource requests blocked, including
// from the `private` address space.
class PrivateNetworkAccessBrowserTestBlockFromUnknown
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestBlockFromUnknown()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromUnknown,
                features::kPrivateNetworkAccessRespectPreflightResults,
            },
            {
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with PNA checks for iframes enabled.
class PrivateNetworkAccessBrowserTestForNavigations
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestForNavigations()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kPrivateNetworkAccessForNavigations,
                features::kPrivateNetworkAccessRespectPreflightResults,
            },
            {
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with PNA checks for navigations enabled in warning-only mode.
class PrivateNetworkAccessBrowserTestForNavigationsWarningOnly
    : public PrivateNetworkAccessBrowserTestForNavigations {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPrivateNetworkAccessForNavigationsWarningOnly};
};

// Test with the feature to send preflights (unenforced) disabled, and insecure
// private network subresource requests blocked.
class PrivateNetworkAccessBrowserTestNoPreflights
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestNoPreflights()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
            },
            {
                features::kPrivateNetworkAccessSendPreflights,
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with the feature to send preflights (enforced) enabled, and insecure
// private network subresource requests blocked.
class PrivateNetworkAccessBrowserTestRespectPreflightResults
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestRespectPreflightResults()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessRespectPreflightResults,
            },
            {
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with PNA checks for worker-related fetches enabled.
class PrivateNetworkAccessBrowserTestForWorkers
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestForWorkers()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessForWorkers,
            },
            {
                features::kPrivateNetworkAccessForWorkersWarningOnly,
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with PNA checks for worker-related fetches enabled and preflight
// enforcement enabled.
class PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessRespectPreflightResults,
                features::kPrivateNetworkAccessForWorkers,
            },
            {
                features::kPrivateNetworkAccessForWorkersWarningOnly,
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with PNA checks for worker-related fetches enabled in warning-only mode,
// including preflights.
class
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkersWarningOnly
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkersWarningOnly()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessRespectPreflightResults,
                features::kPrivateNetworkAccessForWorkers,
                features::kPrivateNetworkAccessForWorkersWarningOnly,
            },
            {
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// Test with insecure private network requests allowed.
class PrivateNetworkAccessBrowserTestNoBlocking
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestNoBlocking()
      : PrivateNetworkAccessBrowserTestBase(
            {},
            {
                features::kBlockInsecurePrivateNetworkRequests,
                features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
                features::kPrivateNetworkAccessForNavigations,
                features::kPrivateNetworkAccessForWorkers,
                features::kPrivateNetworkAccessSendPreflights,
                network::features::kLocalNetworkAccessChecks,
            }) {}
};

// ========================
// INHERITANCE TEST HELPERS
// ========================

namespace {

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

RenderFrameHostImpl* GetFirstChild(RenderFrameHostImpl& parent) {
  CHECK_NE(parent.child_count(), 0ul);
  return parent.child_at(0)->current_frame_host();
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
// TODO(crbug.com/40149351): Revisit this when top-level navigations are
// subject to Private Network Access checks.

// When the `PrivateNetworkAccessForIframes` feature is disabled, iframe fetches
// are not subject to PNA checks.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeFromInsecurePublicToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_GET));
}

// When the `PrivateNetworkAccessForIframes` feature is disabled, iframe fetches
// are not subject to PNA checks.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeFromSecurePublicToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_GET));
}

// This test verifies that when iframe support is enabled in warning-only mode,
// iframe requests:
//  - from an insecure page served from a public IP address
//  - to a loopback IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigationsWarningOnly,
                       IframeFromInsecurePublicToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe fetched successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  EXPECT_THAT(
      InsecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_GET));
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from an insecure page served from a public IP address
//  - to a loopback IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
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
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
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

// This test verifies that when an iframe navigation fails due to PNA, the
// iframe navigates to an error page, even if it had previously committed a
// document.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       FailedNavigationCommitsErrorPage) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  // First add a child frame, which successfully commits a document.
  AddChildFromURL(root_frame_host(), "/empty.html");

  GURL url = InsecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  // Then try to navigate that frame in a way that fails PNA checks.
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

// This test verifies that when iframe support is enabled in warning-only mode,
// iframe requests:
//  - from a secure page served from a public IP address
//  - to a loopback IP address
// are preceded by a preflight request which is allowed to fail.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigationsWarningOnly,
                       IframeFromSecurePublicToLoopbackIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLoopbackURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  // A preflight request first, then the GET request.
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_OPTIONS, METHOD_GET));
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from a secure page served from a public IP address
//  - to a loopback IP address
// are preceded by a preflight request which must succeed.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
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

  // A preflight request only.
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_OPTIONS));
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from a secure page served from a public IP address
//  - to a loopback IP address
// are preceded by a preflight request, to which the server must respond
// correctly.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromSecurePublicToLoopbackIsNotBlocked) {
  GURL initiator_url = SecurePublicURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  GURL url = SecureLoopbackURL(
      MakePnaPathForIframe(url::Origin::Create(initiator_url)));

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  RenderFrameHostImpl* child_frame = GetFirstChild(*root_frame_host());
  EXPECT_EQ(url, EvalJs(child_frame, "document.location.href"));
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());

  // A preflight request first, then the GET request.
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_OPTIONS, METHOD_GET));
}

// Same as above, testing the "treat-as-public-address" CSP directive.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromSecureTreatAsPublicToLoopbackIsNotBlocked) {
  GURL initiator_url = SecureLoopbackURL(kTreatAsPublicAddressPath);
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  GURL url = OtherSecureLoopbackURL(
      MakePnaPathForIframe(url::Origin::Create(initiator_url)));

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  // A preflight request first, then the GET request.
  EXPECT_THAT(
      SecureLoopbackServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_OPTIONS, METHOD_GET));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestForNavigations,
    FormSubmissionFromInsecurePublicToLoopbackIsBlockedInMainFrame) {
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

  // Check that the form submission was blocked.
  EXPECT_FALSE(navigation_manager.was_successful());
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestForNavigations,
    FormSubmissionFromInsecurePublicToLoopbackIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLoopbackURL(kDefaultPath);
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

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestForNavigations,
    FormSubmissionGetFromInsecurePublicToLoopbackIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL target_url = InsecureLoopbackURL(kDefaultPath);

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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
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
  RenderFrameHostImpl* initiator = root_frame_host()->child_at(0)->current_frame_host();

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
