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
#include "content/public/test/private_network_access_util.h"
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
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
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
constexpr char kLocalHost[] = "a.test";
constexpr char kOtherLocalHost[] = "d.test";
constexpr char kPrivateHost[] = "b.test";
constexpr char kPublicHost[] = "c.test";

// Path to a default response served by all servers in this test.
constexpr char kDefaultPath[] = "/defaultresponse";

// Path to a response with the `treat-as-public-address` CSP directive.
constexpr char kTreatAsPublicAddressPath[] =
    "/set-header?Content-Security-Policy: treat-as-public-address";

// Path to a response with a wide-open CORS header. This can be fetched
// cross-origin without triggering CORS violations.
constexpr char kCorsPath[] = "/set-header?Access-Control-Allow-Origin: *";

// Path to a response that passes Private Network Access checks.
constexpr char kPnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *"
    "&Access-Control-Allow-Private-Network: true";

// Path to a cacheable response.
constexpr char kCacheablePath[] = "/cachetime";

// Path to a cacheable variant of `kCorsPath`.
constexpr char kCacheableCorsPath[] =
    "/set-header"
    "?Cache-Control: max-age%3D60"
    "&Access-Control-Allow-Origin: *";

// Path to a cacheable variant of `kPnaPath`.
constexpr char kCacheablePnaPath[] =
    "/set-header"
    "?Cache-Control: max-age%3D60"
    "&Access-Control-Allow-Origin: *"
    "&Access-Control-Allow-Private-Network: true";

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
// NOTE(titouan): The IP address space overrides CLI switch is copied to utility
// processes when said processes are started. Thus if any server is instantiated
// after the network process has started, updates we make to our own CLI
// switches will not propagate to the network process, yielding inconsistent
// results.
class FakeAddressSpaceServer {
 public:
  FakeAddressSpaceServer(net::EmbeddedTestServer::Type type,
                         net::test_server::HttpConnection::Protocol protocol,
                         network::mojom::IPAddressSpace ip_address_space,
                         const base::FilePath& test_data_path)
      : server_(type, protocol) {
    // Use a certificate valid for multiple domains, which we can use to
    // distinguish `local`, `private` and `public` address spaces.
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    server_.SetConnectionListener(&connection_counter_);
    server_.RegisterRequestMonitor(request_observer_.BindCallback());
    server_.RegisterRequestHandler(base::BindRepeating(&HandleRangeRequest));
    server_.AddDefaultHandlers(test_data_path);
    CHECK(server_.Start());

    // Set up the command line in order for this server to be considered a part
    // of `ip_address_space`, irrespective of the actual IP it binds to.
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    std::string switch_str = command_line.GetSwitchValueASCII(
        network::switches::kIpAddressSpaceOverrides);

    // If `switch_str` was empty, we prepend an empty value by unconditionally
    // adding a comma before the new entry. This empty value is ignored by the
    // switch parsing logic.
    base::StrAppend(&switch_str,
                    {
                        ",",
                        server_.host_port_pair().ToString(),
                        "=",
                        IPAddressSpaceToSwitchValue(ip_address_space),
                    });

    command_line.AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                   switch_str);
  }

  // Returns the underlying test server.
  net::EmbeddedTestServer& Get() { return server_; }

  // Returns the total number of sockets accepted by this server.
  int ConnectionCount() const { return connection_counter_.count(); }

  const RequestObserver& request_observer() const { return request_observer_; }

 private:
  static std::string_view IPAddressSpaceToSwitchValue(
      network::mojom::IPAddressSpace space) {
    switch (space) {
      case network::mojom::IPAddressSpace::kLocal:
        return "local";
      case network::mojom::IPAddressSpace::kPrivate:
        return "private";
      case network::mojom::IPAddressSpace::kPublic:
        return "public";
      default:
        ADD_FAILURE() << "Unhandled address space " << space;
        return "";
    }
  }

  ConnectionCounter connection_counter_;
  RequestObserver request_observer_;
  net::EmbeddedTestServer server_;
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
// `private` and `public` address spaces, even though all responses are actually
// served from localhost. Combined with host resolver rules, this lets us define
// three different domains that map to the different address spaces:
//
//  - `a.test` is `local`
//  - `b.test` is `private`
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
      : insecure_local_server_(
            net::EmbeddedTestServer::TYPE_HTTP,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kLocal,
            GetTestDataFilePath()),
        insecure_private_server_(
            net::EmbeddedTestServer::TYPE_HTTP,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kPrivate,
            GetTestDataFilePath()),
        insecure_public_server_(
            net::EmbeddedTestServer::TYPE_HTTP,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kPublic,
            GetTestDataFilePath()),
        secure_local_server_(net::EmbeddedTestServer::TYPE_HTTPS,
                             net::test_server::HttpConnection::Protocol::kHttp1,
                             network::mojom::IPAddressSpace::kLocal,
                             GetTestDataFilePath()),
        secure_private_server_(
            net::EmbeddedTestServer::TYPE_HTTPS,
            net::test_server::HttpConnection::Protocol::kHttp1,
            network::mojom::IPAddressSpace::kPrivate,
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
    host_resolver()->AddRule(kLocalHost, "127.0.0.1");
    host_resolver()->AddRule(kOtherLocalHost, "127.0.0.1");
    host_resolver()->AddRule(kPrivateHost, "127.0.0.1");
    host_resolver()->AddRule(kPublicHost, "127.0.0.1");
  }

  const FakeAddressSpaceServer& InsecureLocalServer() const {
    return insecure_local_server_;
  }

  const FakeAddressSpaceServer& InsecurePrivateServer() const {
    return insecure_private_server_;
  }

  const FakeAddressSpaceServer& InsecurePublicServer() const {
    return insecure_public_server_;
  }

  const FakeAddressSpaceServer& SecureLocalServer() const {
    return secure_local_server_;
  }

  const FakeAddressSpaceServer& SecurePrivateServer() const {
    return secure_private_server_;
  }

  const FakeAddressSpaceServer& SecurePublicServer() const {
    return secure_public_server_;
  }

  GURL InsecureLocalURL(const std::string& path) {
    return insecure_local_server_.Get().GetURL(kLocalHost, path);
  }

  GURL InsecurePrivateURL(const std::string& path) {
    return insecure_private_server_.Get().GetURL(kPrivateHost, path);
  }

  GURL InsecurePublicURL(const std::string& path) {
    return insecure_public_server_.Get().GetURL(kPublicHost, path);
  }

  GURL SecureLocalURL(const std::string& path) {
    return secure_local_server_.Get().GetURL(kLocalHost, path);
  }

  GURL OtherSecureLocalURL(const std::string& path) {
    return secure_local_server_.Get().GetURL(kOtherLocalHost, path);
  }

  GURL SecurePrivateURL(const std::string& path) {
    return secure_private_server_.Get().GetURL(kPrivateHost, path);
  }

  GURL SecurePublicURL(const std::string& path) {
    return secure_public_server_.Get().GetURL(kPublicHost, path);
  }

  GURL NullIPURL(const std::string& path) {
    return insecure_public_server_.Get().GetURL("0.0.0.0", path);
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  FakeAddressSpaceServer insecure_local_server_;
  FakeAddressSpaceServer insecure_private_server_;
  FakeAddressSpaceServer insecure_public_server_;
  FakeAddressSpaceServer secure_local_server_;
  FakeAddressSpaceServer secure_private_server_;
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
                blink::features::kPlzDedicatedWorker,
                features::kBlockInsecurePrivateNetworkRequests,
                features::kPrivateNetworkAccessSendPreflights,
            },
            {}) {}
};

// This definition is required as raw initializer lists aren't allowed in the
// ternary ? operator.
using FeatureVec = std::vector<base::test::FeatureRef>;

// This is the same as PrivateNetworkAccessBrowserTest, but runs each test
// twice, once with kOriginKeyedProcessesByDefault explicitly enabled, and once
// with it explicitly disabled. The tests implemented using this class involve
// sandboxed data: frames whose SiteInfo creation may vary depending on whether
// the feature is enabled or not.
class PrivateNetworkAccessSandboxedDataBrowserTest
    : public PrivateNetworkAccessBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivateNetworkAccessSandboxedDataBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            GetParam() ? FeatureVec({
                             blink::features::kPlzDedicatedWorker,
                             features::kBlockInsecurePrivateNetworkRequests,
                             features::kPrivateNetworkAccessSendPreflights,
                             features::kOriginKeyedProcessesByDefault,
                         })
                       : FeatureVec({
                             blink::features::kPlzDedicatedWorker,
                             features::kBlockInsecurePrivateNetworkRequests,
                             features::kPrivateNetworkAccessSendPreflights,
                         }),
            GetParam()
                ? FeatureVec({})
                : FeatureVec({features::kOriginKeyedProcessesByDefault})) {}
};

class PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption
    : public PrivateNetworkAccessBrowserTest,
      public testing::WithParamInterface<bool> {};

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
            {}) {}
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
            {}) {}
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
            {}) {}
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
            {}) {}
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
            {}) {}
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
            }) {}
};

// ===========================
// CLIENT SECURITY STATE TESTS
// ===========================
//
// These tests verify the contents of `ClientSecurityState` for top-level
// documents in various different circumstances.

// This test verifies the contents of the ClientSecurityState for the initial
// empty document in a new main frame created by the browser.
//
// Note: the renderer-created main frame case is exercised by the
// OpeneeInherits* tests below.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInitialEmptyDoc) {
  // Start a navigation. This forces the RenderFrameHost to initialize its
  // RenderFrame. The navigation is then cancelled by a HTTP 204 code.
  // We're left with a RenderFrameHost containing the default
  // ClientSecurityState values.
  //
  // Serve the response from a secure public server, to confirm that none of
  // the connection's properties are reflected in the committed document, which
  // is not a secure context and belongs to the `local` address space.
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
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies the contents of the ClientSecurityState for `about:blank`
// in a new main frame created by the browser.
//
// Note: the renderer-created main frame case is exercised by the Openee
// inheritance tests below.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForAboutBlank) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForDataURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForFileURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "empty.html")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecureLocalAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecurePrivateAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForInsecurePublicAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSecureLocalAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSecurePrivateAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSecurePublicAddress) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Tests that a top-level navigation to 0.0.0.0 is in the kLocal address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

class PrivateNetworkAccessBrowserTestNullIPKillswitch
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessBrowserTestNullIPKillswitch()
      : PrivateNetworkAccessBrowserTestBase(
            {
                network::features::kTreatNullIPAsPublicAddressSpace,
            },
            {}) {}
};

// Tests that a top-level navigation to 0.0.0.0 is in the kPublic address space
// when a killswitch is enabled to specifically treat it as public.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNullIPKillswitch,
                       ClientSecurityStateForNullIPKillswitch) {
  if constexpr (BUILDFLAG(IS_WIN)) {
    GTEST_SKIP() << "0.0.0.0 behavior varies across platforms and is "
                    "unreachable on Windows.";
  }

  EXPECT_TRUE(NavigateToURL(shell(), NullIPURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForTreatAsPublicAddress) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForTreatAsPublicAddressReportOnly) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      SecureLocalURL("/set-header?Content-Security-Policy-Report-Only: "
                     "treat-as-public-address")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForCachedSecureLocalDocument) {
  // Navigate to the cacheable document in order to cache it, then navigate
  // away.
  const GURL url = SecureLocalURL(kCacheablePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

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
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForCachedInsecurePublicDocument) {
  // Navigate to the cacheable document in order to cache it, then navigate
  // away.
  const GURL url = InsecurePublicURL(kCacheablePath);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

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

// This test verifies that the chrome:// scheme is considered local for the
// purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeURL) {
  // Not all chrome:// hosts are available in content/ but ukm is one of them.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("chrome://ukm")));
  EXPECT_TRUE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeUIScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// The view-source:// scheme should only ever appear in the display URL. It
// shouldn't affect the IPAddressSpace computation. This test verifies that we
// end up with the response IPAddressSpace.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

// Variation of above test with a private address.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeViewSourcePrivate) {
  const GURL url = SecurePrivateURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("view-source:" + url.spec())));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kViewSourceScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

// The chrome-error:// scheme should only ever appear in origins. It shouldn't
// affect the IPAddressSpace computation. This test verifies that we end up with
// the response IPAddressSpace. Error pages should not be considered secure
// contexts however.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

// Variation of above test with a private address.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       ClientSecurityStateForSpecialSchemeChromeErrorPrivate) {
  EXPECT_FALSE(NavigateToURL(shell(), SecurePrivateURL("/empty404.html")));

  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
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

// Convenience overload for absolute URLs.
RenderFrameHostImpl* AddChildFromURL(RenderFrameHostImpl* parent,
                                     const GURL& url) {
  return AddChildFromURL(parent, url.spec());
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

}  // namespace

// ===============================
// ADDRESS SPACE INHERITANCE TESTS
// ===============================
//
// These tests verify that `ClientSecurityState.ip_address_space` is correctly
// inherited by child iframes and openee documents for a variety of URLs with
// local schemes.

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
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
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`
// inherits its address space from the opener. In this case, the opener's
// address space is `public`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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
// address space is `local`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window targeting `about:blank`,
// opened with the "noopener" feature, has its address space set to `local`
// regardless of the address space of the opener.
//
// Compare and contrast against the above tests without "noopener".
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForAboutBlankIsLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window =
      OpenWindowFromAboutBlankNoOpener(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
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
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document inherits its address space from the opener. In this case, the
// opener's address space is `public`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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
// opener's address space is `local`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// This test verifies that a newly-opened window containing the initial empty
// document, opened with the "noopener" feature, has its address space set to
// `local` regardless of the address space of the opener.
//
// Compare and contrast against the above tests without "noopener".
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForInitialEmptyDocIsLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window =
      OpenWindowInitialEmptyDocNoOpener(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
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
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessSandboxedDataBrowserTest,
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

IN_PROC_BROWSER_TEST_P(PrivateNetworkAccessSandboxedDataBrowserTest,
                       SandboxedIframeInheritsAddressSpaceForDataURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(
      root_frame_host(), "var injectedCodeWasExecuted = true");
  ASSERT_NE(nullptr, window);

  // The Javascript in the URL got executed in the new window.
  EXPECT_EQ(true, EvalJs(window, "injectedCodeWasExecuted"));

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeNoOpenerAddressSpaceForJavascriptURLIsLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURLNoOpener(
      root_frame_host(), "var injectedCodeWasExecuted = true");
  ASSERT_NE(nullptr, window);

  // The Javascript in the URL was not executed in the new window. This ensures
  // it is safe to classify the new window as `local` without allowing the
  // opener to execute arbitrary JS in the `local` address space.
  EXPECT_EQ("undefined", EvalJs(window, "typeof injectedCodeWasExecuted"));

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       SandboxedIframeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

// ================================
// SECURE CONTEXT INHERITANCE TESTS
// ================================
//
// These tests verify that `ClientSecurityState.is_web_secure_context` is
// correctly inherited by child iframes and openee documents for a variety of
// URLs with local schemes.

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessSandboxedDataBrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    IframeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    OpeneeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

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

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestDisableWebSecurity,
                       PrivateNetworkPolicyIsAllowInsecure) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestDisableWebSecurity,
                       PrivateNetworkPolicyIsAllowSecure) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that with the blocking feature disabled, the private
// network request policy used by RenderFrameHostImpl is to warn about requests
// from non-secure contexts.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       PrivateNetworkPolicyIsWarnByDefault) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kWarn);
}

// This test verifies that with the blocking feature disabled, the private
// network request policy used by RenderFrameHostImpl is to send unenforced
// preflight requests from secure contexts.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       PrivateNetworkPolicyIsWarnByDefaultForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption,
    testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(,
                         PrivateNetworkAccessSandboxedDataBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "OriginKeyedProcessesByDefault_disabled"
                                      : "OriginKeyedProcessesByDefault_enabled";
                         });

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to block requests from non-secure
// contexts in the `public` address space.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption,
    PrivateNetworkPolicyIsBlockByDefaultForInsecurePublic) {
  PolicyTestContentBrowserClient client;
  bool block_instead_of_warn = GetParam();
  if (block_instead_of_warn) {
    client.SetBlockInsteadOfWarn();
  }

  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to allow requests from non-secure
// contexts in the `private` address space with a warning.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption,
    PrivateNetworkPolicyForInsecurePrivate) {
  PolicyTestContentBrowserClient client;
  bool block_instead_of_warn = GetParam();
  if (block_instead_of_warn) {
    client.SetBlockInsteadOfWarn();
  }

  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            block_instead_of_warn
                ? network::mojom::PrivateNetworkRequestPolicy::kBlock
                : network::mojom::PrivateNetworkRequestPolicy::kWarn);
}

// This test verifies that when the right feature is enabled, the private
// network request policy used by RenderFrameHostImpl for requests is set to
// block requests from non-secure contexts in the private address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockFromPrivate,
                       PrivateNetworkPolicyIsBlockForInsecurePrivate) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to allow requests from non-secure
// contexts in the `unknown` address space.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption,
    PrivateNetworkPolicyIsAllowByDefaultForInsecureUnknown) {
  PolicyTestContentBrowserClient client;
  bool block_instead_of_warn = GetParam();
  if (block_instead_of_warn) {
    client.SetBlockInsteadOfWarn();
  }

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that when the right feature is enabled, the private
// network request policy used by RenderFrameHostImpl for requests is set to
// block requests from non-secure contexts in the `unknown` address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockFromUnknown,
                       PrivateNetworkPolicyIsBlockForInsecureUnknown) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to allow requests from secure
// contexts.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       PrivateNetworkPolicyIsAllowByDefaultForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// This test verifies that when sending preflights is enabled, the private
// network request policy for secure contexts is `kPreflightWarn`.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption,
    PrivateNetworkPolicyForSecureContexts) {
  PolicyTestContentBrowserClient client;
  bool block_instead_of_warn = GetParam();
  if (block_instead_of_warn) {
    client.SetBlockInsteadOfWarn();
  }

  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            block_instead_of_warn
                ? network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock
                : network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
}

// This test verifies that blocking insecure private network requests from the
// `kPublic` address space takes precedence over sending preflight requests.
IN_PROC_BROWSER_TEST_P(
    PrivateNetworkAccessBrowserTestWithBlockInsteadOfWarnOption,
    PrivateNetworkPolicyIsBlockForInsecurePublic) {
  PolicyTestContentBrowserClient client;
  bool block_instead_of_warn = GetParam();
  if (block_instead_of_warn) {
    client.SetBlockInsteadOfWarn();
  }

  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that when enforcing preflights is enabled, the private
// network request policy for secure contexts is `kPreflightBlock`.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PrivateNetworkPolicyIsPreflightBlockForSecureContexts) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock);
}

// This test verifies that when enforcing preflights is enabled, the private
// network request policy for non-secure contexts in the `kPrivate` address
// space is `kPreflightBlock`.
// This checks that as long as the "block from insecure private" feature flag
// is not enabled, we will only show warnings for these requests.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PrivateNetworkPolicyIsWarnForInsecurePrivate) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kWarn);
}

// This test verifies that blocking insecure private network requests from the
// `kPublic` address space takes precedence over enforcing preflight requests.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PrivateNetworkPolicyIsBlockForInsecurePublic) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that child frames with distinct origins from their parent
// do not inherit their private network request policy, which is based on the
// origin of the child document instead.
// TODO (crbug.com/324679506) : Fix the test.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    DISABLED_PrivateNetworkRequestPolicyCalculatedPerOrigin) {
  GURL url = InsecurePublicURL(kDefaultPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddChildFromURL(root_frame_host(), InsecureLocalURL(kDefaultPath));

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// This test verifies that the initial empty document, which inherits its origin
// from the document creator, also inherits its private network request policy.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyInheritedWithOriginForInitialEmptyDoc) {
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
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyInheritedWithOriginForAboutBlank) {
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
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyNotInheritedWithOriginForDataURL) {
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
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyNotInheritedForSandboxedInitialEmptyDoc) {
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
    PrivateNetworkAccessBrowserTest,
    PrivateNetworkRequestPolicyNotInheritedForSandboxedAboutBlank) {
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
// policy of `kAllow` irrespective of the navigation initiator.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       PrivateNetworkRequestPolicyIsAllowForErrorPage) {
  GURL url = InsecurePublicURL(kDefaultPath);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame =
      AddChildFromURL(root_frame_host(), "/close-socket");

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// ==================================================
// SECURE CONTEXT RESTRICTION DEPRECATION TRIAL TESTS
// ==================================================
//
// These tests verify the correct behavior of `private_network_request_policy`
// in the face of the `PrivateNetworkAccessNonSecureContextsAllowed` deprecation
// trial.

// Test with insecure private network requests blocked, excluding navigations.
class PrivateNetworkAccessDeprecationTrialDisabledBrowserTest
    : public PrivateNetworkAccessBrowserTestBase {
 public:
  PrivateNetworkAccessDeprecationTrialDisabledBrowserTest()
      : PrivateNetworkAccessBrowserTestBase(
            {
                features::kBlockInsecurePrivateNetworkRequests,
            },
            {
                features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial,
            }) {}
};

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessDeprecationTrialDisabledBrowserTest,
                       OriginEnabledDoesNothing) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialOriginEnabled) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialOriginDisabled) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.DisabledUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialSettingInheritedByInitialEmptyDoc) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  // TODO(crbug.com/40058599): Expect `kAllow` here once inheritance is
  // properly implemented.
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialSettingInheritedByAboutBlank) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  // TODO(crbug.com/40058599): Expect `kAllow` here once inheritance is
  // properly implemented.
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

// `data:` URLs do not inherit their navigation initiator's origin, so they
// should not inherit deprecation trials.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialSettingNotInheritedByDataURL) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialSettingNotInheritedBySandboxedIframe) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DeprecationTrialSettingNotInheritedByErrorPage) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledUrl()));

  RenderFrameHostImpl* child_frame =
      AddChildFromURL(root_frame_host(), "/close-socket");

  // The iframe committed an error page.
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  // TODO(crbug.com/40747546): Expect `kBlock` once error pages have
  // stricter policies, or decide that this is right and remove this test.
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);
}

// =======================
// SUBRESOURCE FETCH TESTS
// =======================
//
// These tests verify the behavior of the browser when fetching subresources
// across IP address spaces. When the right features are enabled, private
// network requests are blocked.

// This test mimics the tests below, with all blocking features disabled. It
// verifies that by default requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       PrivateNetworkRequestIsNotBlockedByDefault) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// Check that the `--disable-web-security` command-line switch disables PNA
// checks.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestDisableWebSecurity,
                       PrivateNetworkRequestIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecureTreatAsPublicToLocalIsNotBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(OtherSecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       FromSecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource.
  //
  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
//  - when the target server does not respond OK to the preflight request
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page served from a private IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       FromSecurePrivateToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a private IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePrivateToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  // Check that the page can load a local resource.
  //
  // We load it from a secure origin to avoid running afoul of mixed content
  // restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a private IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePrivateToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  // We load the resource from a secure origin to avoid running afoul of mixed
  // content restrictions.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(SecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are disabled, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoPreflights,
                       FromSecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(OtherSecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(OtherSecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(OtherSecureLocalURL(kCorsPath))));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
//  - for which the target server responds OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePublicToLocalPreflightOK) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kPnaPath))));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served from a local IP address
//  - to a local IP address
//  - for which the target server responds OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePublicToLocalPreflightOK) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  // Check that the page can load a local resource. We load it from a secure
  // origin to avoid running afoul of mixed content restrictions.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kPnaPath))));
}

// TODO(crbug.com/40221632): Re-enable this test
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       DISABLED_PreflightConnectionReusedHttp1) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(SecureLocalURL(kPnaPath))));

  // Expect 3 requests, but only 2 connections:
  //
  // 1. The initial request opens a connection, then is cancelled when the
  //    private network request is detected.
  // 2. The preflight request likely reuses this connection.
  // 3. The actual request opens a new connection (the embedded test server does
  //    not handle keep-alives).
  //
  // The socket opened by the initial request is returned to the socket pools
  // asynchronously, so there is the potential for a race condition here, if
  // the following requests are sent very quickly. Sometimes the preflight
  // request might not reuse the initial connection and opens its own. In those
  // cases, it is extremely likely that the final request will reuse the first
  // socket.
  //
  // TODO(crbug.com/40221632): Find out why the connection is not re-used
  // on Mac 11. Likely culprit is some kind of race condition, since the socket
  // closure during 1) above is not synchronized with 2) and 3).
#if BUILDFLAG(IS_MAC)
  int connection_count = SecureLocalServer().ConnectionCount();
  EXPECT_GE(connection_count, 2);  // At least 2 connections.
  EXPECT_LE(connection_count, 3);  // No more than 3 connections.
#else
  EXPECT_EQ(SecureLocalServer().ConnectionCount(), 2);
#endif
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       PreflightConnectionReusedHttp2) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  ConnectionCounter counter;
  net::EmbeddedTestServer http2_server(
      net::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2);
  http2_server.SetConnectionListener(&counter);
  http2_server.AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(http2_server.Start());

  EXPECT_EQ(true,
            EvalJs(root_frame_host(),
                   FetchSubresourceScript(http2_server.GetURL(kPnaPath))));

  EXPECT_EQ(counter.count(), 1);
}

// This test verifies that when the right feature is enabled but the content
// browser client overrides it, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    FromInsecureTreatAsPublicToLocalWithPolicySetToAllowIsNotBlocked) {
  GURL url = InsecureLocalURL(kTreatAsPublicAddressPath);

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecureTreatAsPublicToLocalIsBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a public IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is disabled, requests:
//  - from an insecure page served by a private IP address
//  - to local IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecurePrivateToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a private IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestBlockFromPrivate,
                       FromInsecurePrivateToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePrivateURL(kDefaultPath)));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false, EvalJs(root_frame_host(),
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a local IP address
//  - to local IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page can load a local resource.
  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - embedded in an insecure page served from a local IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    FromSecurePublicEmbeddedInInsecureLocalToLocalIsBlocked) {
  // First navigate to an insecure page served by a local IP address.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

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

  // Check that the iframe cannot load a local resource.
  EXPECT_EQ(false, EvalJs(child_frame,
                          FetchSubresourceScript(InsecureLocalURL(kCorsPath))));
}

// This test verifies that even when the right feature is enabled, requests:
//  - from a non-secure context in the `local` IP address space
//  - to a subresource cached from a `local` IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecureLocalToCachedLocalIsNotBlocked) {
  GURL target = InsecureLocalURL(kCacheablePath);

  // Cache the resource first. The server receives a GET request.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  // Check that the page can still load the subresource from cache. The server
  // does not receive any new request.
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a non-secure context in the `public` IP address space
//  - to a subresource cached from a `local` IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromInsecurePublicToCachedLocalIsBlocked) {
  GURL target = InsecureLocalURL(kCacheablePath);

  // Cache the resource first, by fetching it from a document in the same IP
  // address space. The server receives a GET request.
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  // Now navigate to a document in the `public` address space belonging to the
  // same site as the previous document (this will use the same cache key).
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load the resource, even from cache. The server
  // does not receive any new request.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      InsecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure context in the `local` IP address space
//  - to a subresource cached from a `local` IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecureLocalToCachedLocalIsNotBlocked) {
  GURL target = SecureLocalURL(kCacheablePath);

  // Cache the resource first. The server receives a GET request.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  // Check that the page can still load the subresource from cache. The server
  // does not receive any new request.
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));
}

// This test verifies that when preflights are sent but not enforced, requests:
//  - from a secure page served in the `public` IP address space
//  - to a subresource cached from a `local` IP address
//  - for which the target server does not respond OK to the preflight request
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FromSecurePublicToCachedLocalIsNotBlocked) {
  GURL target = OtherSecureLocalURL(kCacheableCorsPath);

  // Cache the resource first.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page can still load the subresource from cache.
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));

  // The server receives a preflight request because the preflight response is
  // not cached, but no second GET request.
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET, METHOD_OPTIONS));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served in the `public` IP address space
//  - to a subresource cached from a `local` IP address
//  - for which the target server does not respond OK to the preflight request
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePublicToCachedLocalIsBlocked) {
  GURL target = OtherSecureLocalURL(kCacheableCorsPath);

  // Cache the resource first.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page cannot load the subresource from cache.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(target)));

  // The server receives a preflight request because the preflight response is
  // not cached, but no second GET request.
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET, METHOD_OPTIONS));
}

// This test verifies that when preflights are sent and enforced, requests:
//  - from a secure page served in the `public` IP address space
//  - to a subresource cached from a `local` IP address
//  - for which the target server responds OK to the preflight request
//  are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestRespectPreflightResults,
                       FromSecurePublicToCachedLocalIsNotBlocked) {
  GURL target = OtherSecureLocalURL(kCacheablePnaPath);

  // Cache the resource first.
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET));

  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // Check that the page can still load the subresource from cache.
  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));

  // The server receives a preflight request because the preflight response is
  // not cached, but no second GET request.
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target),
      ElementsAre(METHOD_GET, METHOD_OPTIONS));
}

// This test verifies that even with the blocking feature disabled, an insecure
// page in the `local` address space cannot fetch a `file:` URL.
//
// This is relevant to Private Network Access, since `file:` URLs are considered
// to be in the `local` IP address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       InsecurePageCannotRequestFile) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(GetTestUrl(
                                                 "", "empty.html"))));
}

// This test verifies that even with the blocking feature disabled, a secure
// page in the `local` address space cannot fetch a `file:` URL.
//
// This is relevant to Private Network Access, since `file:` URLs are considered
// to be in the `local` IP address space.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestNoBlocking,
                       SecurePageCannotRequestFile) {
  EXPECT_TRUE(NavigateToURL(shell(), SecureLocalURL(kDefaultPath)));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(false, EvalJs(root_frame_host(), FetchSubresourceScript(GetTestUrl(
                                                 "", "empty.html"))));
}

// This test verifies that if a page redirects after responding to a private
// network request to a server in a different address space, the request does
// not fail.
// Regression test for https://crbug.com/1293891.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest, Redirect) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePrivateURL(kDefaultPath)));

  GURL target =
      SecureLocalURL("/server-redirect?" + SecurePrivateURL(kCorsPath).spec());

  EXPECT_EQ(true, EvalJs(root_frame_host(), FetchSubresourceScript(target)));
}

// This test verifies that if a request is made for a resource of which a
// partial prefix range of bytes was cached, a preflight is correctly sent for
// the non-cached portion, and the renderer does not crash.
// Regression test for https://crbug.com/1279376.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest, PrefixRangePreflight) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  const GURL url = SecureLocalURL("/echorange?this-is-a-test");

  constexpr std::string_view kFetchRangeScript = R"(
    (async () => {
      const url = $1;
      const range = $2;

      const headers = {};
      if (range) {
        headers["Range"] = range;
      }

      const response = await fetch(url, { headers });
      const body = await response.text();
      return body;
    })()
  )";

  // Cache a portion of the target resource.
  EXPECT_EQ("this", EvalJs(root_frame_host(),
                           JsReplace(kFetchRangeScript, url, "bytes=0-3")));

  // The server received a preflight request, followed by a GET request.
  EXPECT_THAT(SecureLocalServer().request_observer().RequestMethodsForUrl(url),
              ElementsAre(net::test_server::METHOD_OPTIONS,
                          net::test_server::METHOD_GET));

  // Fetch the whole resource.
  EXPECT_EQ(
      "this-is-a-test",
      EvalJs(root_frame_host(), JsReplace(kFetchRangeScript, url, "bytes=0-")));

  // The server received a single GET request for the non-cached suffix. The
  // preflight response was previously cached, so there is no second preflight.
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(net::test_server::METHOD_OPTIONS,
                  net::test_server::METHOD_GET, net::test_server::METHOD_GET));
}

// =========================
// WORKER SCRIPT FETCH TESTS
// =========================

namespace {

// Path to a worker script that posts a message to its creator once loaded.
constexpr char kWorkerScriptPath[] = "/workers/post_ready.js";

// Same as above, but with PNA headers set correctly for preflight requests.
constexpr char kWorkerScriptWithPnaHeadersPath[] =
    "/workers/post_ready_with_pna_headers.js";

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

// Same as above, but with PNA headers set correctly for preflight requests.
constexpr char kSharedWorkerScriptWithPnaHeadersPath[] =
    "/workers/shared_post_ready_with_pna_headers.js";

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
  EXPECT_NE("", result.error);
#endif
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FetchWorkerFromInsecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForWorkers,
                       FetchWorkerFromInsecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers,
    FetchWorkerFromInsecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkersWarningOnly,
    FetchWorkerFromInsecurePublicToLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FetchWorkerFromSecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForWorkers,
                       FetchWorkerFromSecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers,
    FetchWorkerFromSecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // The request is exempt from Private Network Access checks because it is
  // same-origin and the origin is potentially trustworthy.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkersWarningOnly,
    FetchWorkerFromSecurePublicToLocalFailedPreflight) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchWorkerScript(kWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers,
    FetchWorkerFromSecureTreatAsPublicToLocalSuccess) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  EXPECT_EQ(true, EvalJs(root_frame_host(),
                         FetchWorkerScript(kWorkerScriptWithPnaHeadersPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FetchSharedWorkerFromInsecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForWorkers,
                       FetchSharedWorkerFromInsecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      false, EvalJs(root_frame_host(),
                    FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers,
    FetchSharedWorkerFromInsecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      false, EvalJs(root_frame_host(),
                    FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkersWarningOnly,
    FetchSharedWorkerFromInsecurePublicToLocal) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       FetchSharedWorkerFromSecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForWorkers,
                       FetchSharedWorkerFromSecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers,
    FetchSharedWorkerFromSecureTreatAsPublicToLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  // The request is exempt from Private Network Access checks because it is
  // same-origin and the origin is potentially trustworthy.
  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkersWarningOnly,
    FetchSharedWorkerFromSecurePublicToLocalFailedPreflight) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  ExpectFetchSharedWorkerScriptResult(
      true, EvalJs(root_frame_host(),
                   FetchSharedWorkerScript(kSharedWorkerScriptPath)));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestRespectPreflightResultsForWorkers,
    FetchSharedWorkerFromSecureTreatAsPublicToLocalSuccess) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureLocalURL(kTreatAsPublicAddressPath)));

  ExpectFetchSharedWorkerScriptResult(
      true,
      EvalJs(root_frame_host(),
             FetchSharedWorkerScript(kSharedWorkerScriptWithPnaHeadersPath)));
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
// TODO(crbug.com/40149351): Revisit this when top-level navigations are
// subject to Private Network Access checks.

// When the `PrivateNetworkAccessForIframes` feature is disabled, iframe fetches
// are not subject to PNA checks.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeFromInsecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  EXPECT_THAT(
      InsecureLocalServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_GET));
}

// When the `PrivateNetworkAccessForIframes` feature is disabled, iframe fetches
// are not subject to PNA checks.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       IframeFromSecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  EXPECT_THAT(SecureLocalServer().request_observer().RequestMethodsForUrl(url),
              ElementsAre(METHOD_GET));
}

// This test verifies that when iframe support is enabled in warning-only mode,
// iframe requests:
//  - from an insecure page served from a public IP address
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigationsWarningOnly,
                       IframeFromInsecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe fetched successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  EXPECT_THAT(
      InsecureLocalServer().request_observer().RequestMethodsForUrl(url),
      ElementsAre(METHOD_GET));
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from an insecure page served from a public IP address
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromInsecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL("/empty.html");

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
      InsecureLocalServer().request_observer().RequestMethodsForUrl(url),
      IsEmpty());
}

// Same as above, testing the "treat-as-public-address" CSP directive.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromInsecureTreatAsPublicToLocalIsBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureLocalURL(kTreatAsPublicAddressPath)));

  GURL url = InsecureLocalURL("/empty.html");

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

  GURL url = InsecureLocalURL("/empty.html");

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
      InsecureLocalServer().request_observer().RequestMethodsForUrl(url),
      IsEmpty());
}

// This test verifies that when iframe support is enabled in warning-only mode,
// iframe requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
// are preceded by a preflight request which is allowed to fail.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigationsWarningOnly,
                       IframeFromSecurePublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLocalURL("/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  EXPECT_TRUE(child_navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(GetFirstChild(*root_frame_host()),
                        "document.location.href"));

  // A preflight request first, then the GET request.
  EXPECT_THAT(SecureLocalServer().request_observer().RequestMethodsForUrl(url),
              ElementsAre(METHOD_OPTIONS, METHOD_GET));
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
// are preceded by a preflight request which must succeed.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromSecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), SecurePublicURL(kDefaultPath)));

  GURL url = SecureLocalURL("/empty.html");

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
  EXPECT_THAT(SecureLocalServer().request_observer().RequestMethodsForUrl(url),
              ElementsAre(METHOD_OPTIONS));
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from a secure page served from a public IP address
//  - to a local IP address
// are preceded by a preflight request, to which the server must respond
// correctly.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromSecurePublicToLocalIsNotBlocked) {
  GURL initiator_url = SecurePublicURL(kDefaultPath);
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  GURL url =
      SecureLocalURL(MakePnaPathForIframe(url::Origin::Create(initiator_url)));

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  RenderFrameHostImpl* child_frame = GetFirstChild(*root_frame_host());
  EXPECT_EQ(url, EvalJs(child_frame, "document.location.href"));
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());

  // A preflight request first, then the GET request.
  EXPECT_THAT(SecureLocalServer().request_observer().RequestMethodsForUrl(url),
              ElementsAre(METHOD_OPTIONS, METHOD_GET));
}

// Same as above, testing the "treat-as-public-address" CSP directive.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTestForNavigations,
                       IframeFromSecureTreatAsPublicToLocalIsNotBlocked) {
  GURL initiator_url = SecureLocalURL(kTreatAsPublicAddressPath);
  EXPECT_TRUE(NavigateToURL(shell(), initiator_url));

  GURL url = OtherSecureLocalURL(
      MakePnaPathForIframe(url::Origin::Create(initiator_url)));

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  AddChildFromURLWithoutWaiting(root_frame_host(), url);
  ASSERT_TRUE(child_navigation_manager.WaitForNavigationFinished());

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  // A preflight request first, then the GET request.
  EXPECT_THAT(SecureLocalServer().request_observer().RequestMethodsForUrl(url),
              ElementsAre(METHOD_OPTIONS, METHOD_GET));
}

IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTestForNavigations,
    FormSubmissionFromInsecurePublicToLocalIsBlockedInMainFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL(kDefaultPath);
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
    FormSubmissionFromInsecurePublicToLocalIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL url = InsecureLocalURL(kDefaultPath);
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
    FormSubmissionGetFromInsecurePublicToLocalIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecurePublicURL(kDefaultPath)));

  GURL target_url = InsecureLocalURL(kDefaultPath);

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
                       SiblingNavigationFromInsecurePublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(shell(), InsecureLocalURL(kDefaultPath)));

  // Named targeting only works if the initiator is one of:
  //
  //  - the target's parent -> uninteresting
  //  - the target's opener -> implies the target is a main frame
  //  - same-origin with the target -> the only option left
  //
  // Thus we use CSP: treat-as-public-address to place the initiator in a
  // different IP address space as its same-origin target.
  GURL initiator_url = InsecureLocalURL(kTreatAsPublicAddressPath);
  GURL target_url = InsecureLocalURL(kDefaultPath);

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
  EXPECT_THAT(
      SecureLocalServer().request_observer().RequestMethodsForUrl(target_url),
      IsEmpty());
}

}  // namespace content
