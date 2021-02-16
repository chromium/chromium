// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/ip_address.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

constexpr char kInsecureHost[] = "foo.test";

constexpr char kDefaultPath[] = "/defaultresponse";

constexpr char kTreatAsPublicAddressPath[] =
    "/set-header?Content-Security-Policy: treat-as-public-address";

GURL SecureURL(const net::EmbeddedTestServer& server, const std::string& path) {
  // http://localhost is considered secure. Relying on this is easier than using
  // the HTTPS test server, since that server cannot lie about its domain name,
  // so we have to use localhost anyway.
  return server.GetURL(path);
}

GURL InsecureURL(const net::EmbeddedTestServer& server,
                 const std::string& path) {
  // The mock resolver is set to resolve anything to 127.0.0.1, so we use
  // http://foo.test as an insecure origin.
  return server.GetURL(kInsecureHost, path);
}

GURL SecureDefaultURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kDefaultPath);
}

GURL InsecureDefaultURL(const net::EmbeddedTestServer& server) {
  return InsecureURL(server, kDefaultPath);
}

GURL SecureTreatAsPublicAddressURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kTreatAsPublicAddressPath);
}

GURL InsecureTreatAsPublicAddressURL(const net::EmbeddedTestServer& server) {
  return InsecureURL(server, kTreatAsPublicAddressPath);
}

// Returns a snippet of Javascript that fetch()es the given URL.
//
// The snippet evaluates to a boolean promise which resolves to true iff the
// fetch was successful. The promise never rejects, as doing so makes it hard
// to assert failure.
std::string FetchSubresourceScript(const std::string& url_spec) {
  return JsReplace(
      R"(fetch($1).then(
           response => response.ok,
           error => {
             console.log('Error fetching ' + $1, error);
             return false;
           });
      )",
      url_spec);
}

// Returns an IP address in the private address space.
net::IPAddress PrivateAddress() {
  return net::IPAddress(10, 0, 1, 2);
}

// Returns an IP address in the public address space.
net::IPAddress PublicAddress() {
  return net::IPAddress(40, 0, 1, 2);
}

// Minimal response headers for an intercepted response to be successful.
constexpr base::StringPiece kMinimalResponseHeaders =  // force line break
    R"(HTTP/1.0 200 OK
Content-type: text/html

)";

// Minimal response headers for an intercepted response to be an error page.
constexpr base::StringPiece kMinimalErrorResponseHeaders =
    "HTTP/1.0 404 Not Found";

// Minimal response body containing an HTML document.
constexpr base::StringPiece kMinimalHtmlBody = R"(
<html>
<head></head>
<body></body>
</html>
)";

// Wraps the URLLoaderInterceptor method of the same name, asserts success.
//
// Note: ASSERT_* macros can only be used in functions returning void.
void WriteResponseBody(base::StringPiece body,
                       network::mojom::URLLoaderClient* client) {
  ASSERT_EQ(content::URLLoaderInterceptor::WriteResponseBody(body, client),
            MOJO_RESULT_OK);
}

// Helper for MaybeInterceptWithFakeEndPoint.
network::mojom::URLResponseHeadPtr BuildInterceptedResponseHead(
    const net::IPEndPoint& endpoint,
    bool should_succeed) {
  auto response = network::mojom::URLResponseHead::New();
  base::StringPiece headers_string =
      should_succeed ? kMinimalResponseHeaders : kMinimalErrorResponseHeaders;
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers_string));
  if (should_succeed)
    response->headers->GetMimeType(&response->mime_type);
  response->remote_endpoint = endpoint;
  return response;
}

// Helper for InterceptorWithFakeEndPointInternal.
bool MaybeInterceptWithFakeEndPoint(
    const GURL& intercepted_url,
    const net::IPEndPoint& endpoint,
    bool should_succeed,
    content::URLLoaderInterceptor::RequestParams* params) {
  const GURL& request_url = params->url_request.url;
  if (request_url != intercepted_url) {
    LOG(INFO) << "MaybeInterceptWithFakeEndPoint: ignoring request to "
              << request_url;
    return false;
  }

  LOG(INFO) << "MaybeInterceptWithFakeEndPoint: intercepting request to "
            << request_url;

  params->client->OnReceiveResponse(
      BuildInterceptedResponseHead(endpoint, should_succeed));
  WriteResponseBody(kMinimalHtmlBody, params->client.get());
  return true;
}

std::unique_ptr<content::URLLoaderInterceptor>
InterceptorWithFakeEndPointInternal(const GURL& url,
                                    const net::IPEndPoint& endpoint,
                                    bool should_succeed) {
  LOG(INFO) << "Starting to intercept requests to " << url
            << " with fake endpoint " << endpoint.ToString();
  return std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
      &MaybeInterceptWithFakeEndPoint, url, endpoint, should_succeed));
}

// The returned interceptor intercepts requests to |url|, fakes its network
// endpoint to reflect the value of |endpoint|, and responds OK with a minimal
// HTML body.
std::unique_ptr<content::URLLoaderInterceptor> InterceptorWithFakeEndPoint(
    const GURL& url,
    const net::IPEndPoint& endpoint) {
  return InterceptorWithFakeEndPointInternal(url, endpoint,
                                             /* should_succeed=*/true);
}

// The returned interceptor intercepts requests to |url|, fakes its network
// endpoint to reflect the value of |endpoint|, and responds with a 404 error.
std::unique_ptr<content::URLLoaderInterceptor> FailInterceptorWithFakeEndPoint(
    const GURL& url,
    const net::IPEndPoint& endpoint) {
  return InterceptorWithFakeEndPointInternal(url, endpoint,
                                             /* should_succeed=*/false);
}

// A |ContentBrowserClient| implementation that allows modifying the return
// value of |ShouldAllowInsecurePrivateNetworkRequests()| at will.
class PolicyTestContentBrowserClient : public TestContentBrowserClient {
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

  bool ShouldAllowInsecurePrivateNetworkRequests(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override {
    return allowlisted_origins_.find(origin) != allowlisted_origins_.end();
  }

 private:
  std::set<url::Origin> allowlisted_origins_;
};

// RAII wrapper for |SetContentBrowserClientForTesting()|.
class ContentBrowserClientRegistration {
 public:
  explicit ContentBrowserClientRegistration(ContentBrowserClient* client)
      : old_client_(SetBrowserClientForTesting(client)) {}

  ~ContentBrowserClientRegistration() {
    SetBrowserClientForTesting(old_client_);
  }

 private:
  ContentBrowserClient* const old_client_;
};

}  // namespace

// It is hard to test this feature fully at the integration test level. Indeed,
// there is no good way to inject a fake endpoint value into the URLLoader code
// that performs the CORS-RFC1918 checks. The most intrusive injection
// primitive, URLLoaderInterceptor, cannot be made to work as it bypasses the
// network service entirely. Intercepted subresource requests therefore do not
// execute the code under test and are never blocked.
//
// We are able to intercept top-level navigations, which allows us to test that
// the correct address space is committed in the client security state for
// local, private and public IP addresses.
//
// We further test that given a client security state with each IP address
// space, subresource requests served by local IP addresses fail unless
// initiated from the same address space. This provides integration testing
// coverage for both success and failure cases of the code under test.
//
// Finally, we have unit tests that test all possible combinations of source and
// destination IP address spaces in services/network/url_loader_unittest.cc.
// Those cover fetches to other address spaces than local.
class CorsRfc1918BrowserTestBase : public ContentBrowserTest {
 public:
  RenderFrameHostImpl* root_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
  }

 protected:
  // Allows subclasses to construct instances with different features enabled.
  explicit CorsRfc1918BrowserTestBase(
      const std::vector<base::Feature>& enabled_features) {
    feature_list_.InitWithFeatures(enabled_features, {});

    StartServer();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // |kInsecureHost| serves as an insecure alternative to `localhost`.
    // This must be called on the main thread lest it segfaults.
    host_resolver()->AddRule(kInsecureHost, "127.0.0.1");
  }

 private:
  // Constructor helper. We cannot use ASSERT_* macros in constructors.
  void StartServer() { ASSERT_TRUE(embedded_test_server()->Start()); }

  base::test::ScopedFeatureList feature_list_;
};

// Test with insecure private network requests blocked, excluding navigations.
class CorsRfc1918BrowserTest : public CorsRfc1918BrowserTestBase {
 public:
  CorsRfc1918BrowserTest()
      : CorsRfc1918BrowserTestBase(
            {features::kBlockInsecurePrivateNetworkRequests}) {}
};

// Test with insecure private network requests blocked, including navigations.
class CorsRfc1918BrowserTestBlockNavigations
    : public CorsRfc1918BrowserTestBase {
 public:
  CorsRfc1918BrowserTestBlockNavigations()
      : CorsRfc1918BrowserTestBase({
            features::kBlockInsecurePrivateNetworkRequests,
            features::kBlockInsecurePrivateNetworkRequestsForNavigations,
        }) {}
};

// Test with insecure private network requests allowed.
class CorsRfc1918BrowserTestNoBlocking : public CorsRfc1918BrowserTestBase {
 public:
  CorsRfc1918BrowserTestNoBlocking() : CorsRfc1918BrowserTestBase({}) {}
};

// This test verifies that when the right feature is enabled, iframe requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTestBlockNavigations,
                       IframeFromInsecureTreatAsPublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  GURL url = InsecureURL(*embedded_test_server(), "/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/empty.html";
    document.body.appendChild(iframe);
  )"));

  child_navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe failed to fetch.
  EXPECT_FALSE(child_navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            EvalJs(child_frame, "document.location.href"));

  // The frame committed an error page but retains the original URL so that
  // reloading the page does the right thing. The committed origin on the other
  // hand is opaque, which it would not be if the navigation had succeeded.
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

// This test mimics the one above, only it is executed without enabling the
// BlockInsecurePrivateNetworkRequestsForNavigations feature. It asserts that
// the navigation is not blocked in this case.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeFromInsecureTreatAsPublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  GURL url = InsecureURL(*embedded_test_server(), "/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/empty.html";
    document.body.appendChild(iframe);
  )"));

  child_navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe navigated successfully.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(url, EvalJs(child_frame, "document.location.href"));
}

// Similar to IframeFromInsecureTreatAsPublicToLocalIsBlocked, but in
// report-only mode. As a result "treat-as-public-address" must be ignored.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CspReportOnlyTreatAsPublicAddressIgnored) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureURL(*embedded_test_server(),
                           "/set-header?Content-Security-Policy-Report-Only: "
                           "treat-as-public-address")));

  GURL url = InsecureURL(*embedded_test_server(), "/empty.html");

  TestNavigationManager child_navigation_manager(shell()->web_contents(), url);

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "/empty.html";
    document.body.appendChild(iframe);
  )"));

  child_navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe was not blocked.
  EXPECT_TRUE(child_navigation_manager.was_successful());

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  RenderFrameHostImpl* child_frame =
      root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(url, EvalJs(child_frame, "document.location.href"));
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  EXPECT_FALSE(child_frame->GetLastCommittedOrigin().opaque());
}

// TODO(https://crbug.com/1129326): Revisit this when main-frame navigations are
// subject to CORS-RFC1918 checks.
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTestBlockNavigations,
    FormSubmissionFromInsecurePublictoLocalIsNotBlockedInMainFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  GURL url = InsecureDefaultURL(*embedded_test_server());
  TestNavigationManager navigation_manager(shell()->web_contents(), url);

  base::StringPiece script_template = R"(
    const form = document.createElement("form");
    form.action = $1;
    form.method = "post";
    document.body.appendChild(form);
    form.submit();
  )";

  EXPECT_TRUE(ExecJs(root_frame_host(), JsReplace(script_template, url)));

  navigation_manager.WaitForNavigationFinished();

  // Check that the child iframe was not blocked.
  EXPECT_TRUE(navigation_manager.was_successful());

  EXPECT_EQ(url, EvalJs(root_frame_host(), "document.location.href"));
  EXPECT_EQ(url, root_frame_host()->GetLastCommittedURL());
  EXPECT_FALSE(root_frame_host()->GetLastCommittedOrigin().opaque());
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTestBlockNavigations,
    FormSubmissionFromInsecurePublictoLocalIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  GURL url = InsecureDefaultURL(*embedded_test_server());
  TestNavigationManager navigation_manager(shell()->web_contents(), url);

  base::StringPiece script_template = R"(
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

  navigation_manager.WaitForNavigationFinished();

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
    CorsRfc1918BrowserTestBlockNavigations,
    FormSubmissionGetFromInsecurePublictoLocalIsBlockedInChildFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  GURL target_url = InsecureDefaultURL(*embedded_test_server());

  // The page navigates to `url` followed by an empty query: '?'.
  GURL expected_url = GURL(target_url.spec() + "?");
  TestNavigationManager navigation_manager(shell()->web_contents(),
                                           expected_url);

  base::StringPiece script_template = R"(
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

  navigation_manager.WaitForNavigationFinished();

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

// TODO(https://crbug.com/1134601): `about:` URLs are all treated as `kUnknown`
// today. This is ~incorrect, but safe, as their web-facing behavior will be
// equivalent to "public".
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForAboutURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            security_state->ip_address_space);

  EXPECT_EQ("public", EvalJs(root_frame_host(), "document.addressSpace"));
}

// TODO(https://crbug.com/1134601): `data:` URLs are all treated as `kUnknown`
// today. This is ~incorrect, but safe, as their web-facing behavior will be
// equivalent to "public".
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForDataURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            security_state->ip_address_space);

  EXPECT_EQ("public", EvalJs(root_frame_host(), "document.addressSpace"));
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForFileURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "empty.html")));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);

  EXPECT_EQ("local", EvalJs(root_frame_host(), "document.addressSpace"));
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForInsecureLocalAddress) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);

  EXPECT_EQ("local", EvalJs(root_frame_host(), "document.addressSpace"));
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForSecureLocalAddress) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);

  EXPECT_EQ("local", EvalJs(root_frame_host(), "document.addressSpace"));
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForTreatAsPublicAddress) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  EXPECT_EQ("public", EvalJs(root_frame_host(), "document.addressSpace"));
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForPrivateAddress) {
  // Intercept the page load and pretend it came from a public IP.

  const GURL url = InsecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PrivateAddress(), embedded_test_server()->port()));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);

  EXPECT_EQ("private", EvalJs(root_frame_host(), "document.addressSpace"));
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForPublicAddress) {
  // Intercept the page load and pretend it came from a public IP.

  const GURL url = InsecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PublicAddress(), embedded_test_server()->port()));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  EXPECT_EQ("public", EvalJs(root_frame_host(), "document.addressSpace"));
}

// This test verifies that the chrome:// scheme is considered local for the
// purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       CommitsClientSecurityStateForSpecialSchemeChromeURL) {
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
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    CommitsClientSecurityStateForSpecialSchemeViewSourcePublic) {
  // Intercept the page load and pretend it came from a public IP.
  const GURL url = SecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PublicAddress(), embedded_test_server()->port()));

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
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    CommitsClientSecurityStateForSpecialSchemeViewSourcePrivate) {
  // Intercept the page load and pretend it came from a private IP.
  const GURL url = SecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PrivateAddress(), embedded_test_server()->port()));

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
// the response IPAddressSpace.
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    CommitsClientSecurityStateForSpecialSchemeChromeErrorPublic) {
  // Intercept the page load and pretend it came from a public IP.
  const GURL url = SecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = FailInterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PublicAddress(), embedded_test_server()->port()));

  EXPECT_FALSE(NavigateToURL(shell(), url));
  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

// Variation of above test with a private address.
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    CommitsClientSecurityStateForSpecialSchemeChromeErrorPrivate) {
  // Intercept the page load and pretend it came from a public IP.
  const GURL url = SecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = FailInterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PrivateAddress(), embedded_test_server()->port()));

  EXPECT_FALSE(NavigateToURL(shell(), url));
  EXPECT_FALSE(
      root_frame_host()->GetLastCommittedURL().SchemeIs(kChromeErrorScheme));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);
}

namespace {

// Helper for CreateBlobURL() and CreateFilesystemURL().
// ASSERT_* macros can only be used in functions returning void.
void AssertResultIsString(const EvalJsResult& result) {
  // We could skip this assert, but it helps in case of error.
  ASSERT_EQ("", result.error);
  // We could use result.value.is_string(), but this logs the actual type in
  // case of mismatch.
  ASSERT_EQ(base::Value::Type::STRING, result.value.type()) << result.value;
}

// Creates a blob containing dummy HTML, then returns its URL.
// Executes javascript to do so in |frame_host|, which must not be nullptr.
GURL CreateBlobURL(RenderFrameHostImpl* frame_host) {
  EvalJsResult result = EvalJs(frame_host, R"(
    const blob = new Blob(["foo"], {type: "text/html"});
    URL.createObjectURL(blob)
  )");

  AssertResultIsString(result);
  return GURL(result.ExtractString());
}

// Writes some dummy HTML to a file, then returns its `filesystem:` URL.
// Executes javascript to do so in |frame_host|, which must not be nullptr.
GURL CreateFilesystemURL(RenderFrameHostImpl* frame_host) {
  EvalJsResult result = EvalJs(frame_host, R"(
    // It seems anonymous async functions are not available yet, so we cannot
    // use an immediately-invoked function expression.
    async function run() {
      const fs = await new Promise((resolve, reject) => {
        window.webkitRequestFileSystem(window.TEMPORARY, 1024, resolve, reject);
      });
      const file = await new Promise((resolve, reject) => {
        fs.root.getFile('hello.html', {create: true}, resolve, reject);
      });
      const writer = await new Promise((resolve, reject) => {
        file.createWriter(resolve, reject);
      });
      await new Promise((resolve) => {
        writer.onwriteend = resolve;
        writer.write(new Blob(["foo"], {type: "text/html"}));
      });
      return file.toURL();
    }
    run()
  )");

  AssertResultIsString(result);
  return GURL(result.ExtractString());
}

// Helper for AddChildWithScript().
// ASSERT_* macros can only be used in functions returning void.
void AssertChildCountEquals(RenderFrameHostImpl* parent, size_t count) {
  ASSERT_EQ(parent->child_count(), count);
}

// Executes |script| to add a new child iframe to the given |parent| document.
//
// |parent| must not be nullptr.
// |script| must return true / resolve to true upon success.
//
// Returns a pointer to the child frame host.
RenderFrameHostImpl* AddChildWithScript(RenderFrameHostImpl* parent,
                                        const std::string& script) {
  size_t initial_child_count = parent->child_count();

  EvalJsResult result = EvalJs(parent, script);
  EXPECT_EQ(true, result);  // For the error message.

  AssertChildCountEquals(parent, initial_child_count + 1);
  return parent->child_at(initial_child_count)->current_frame_host();
}

// Adds a child iframe sourced from |url| to the given |parent| document.
//
// |parent| must not be nullptr.
RenderFrameHostImpl* AddChildFromURL(RenderFrameHostImpl* parent,
                                     const GURL& url) {
  std::string script_template = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )";
  return AddChildWithScript(parent, JsReplace(script_template, url));
}

RenderFrameHostImpl* AddChildFromAboutBlank(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, GURL("about:blank"));
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
  return AddChildFromURL(parent, GURL("data:text/html,foo"));
}

RenderFrameHostImpl* AddChildFromJavascriptURL(RenderFrameHostImpl* parent) {
  return AddChildFromURL(parent, GURL("javascript:'foo'"));
}

RenderFrameHostImpl* AddChildFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return AddChildFromURL(parent, blob_url);
}

RenderFrameHostImpl* AddChildFromFilesystem(RenderFrameHostImpl* parent) {
  GURL fs_url = CreateFilesystemURL(parent);
  return AddChildFromURL(parent, fs_url);
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

RenderFrameHostImpl* AddSandboxedChildFromFilesystem(
    RenderFrameHostImpl* parent) {
  GURL fs_url = CreateFilesystemURL(parent);
  return AddSandboxedChildFromURL(parent, fs_url);
}

// Returns the main frame RenderFrameHostImpl in the given |shell|.
//
// |shell| must not be nullptr.
//
// Helper for OpenWindow*().
RenderFrameHostImpl* GetMainFrameHostImpl(Shell* shell) {
  return static_cast<RenderFrameHostImpl*>(
      shell->web_contents()->GetMainFrame());
}

// Opens a new window from within |parent|, pointed at the given |url|.
// Waits until the openee window has navigated to |url|, then returns a pointer
// to its main frame RenderFrameHostImpl.
//
// |parent| must not be nullptr.
RenderFrameHostImpl* OpenWindowFromURL(RenderFrameHostImpl* parent,
                                       const GURL& url) {
  return GetMainFrameHostImpl(OpenPopup(parent, url, "child"));
}

RenderFrameHostImpl* OpenWindowFromAboutBlank(RenderFrameHostImpl* parent) {
  return OpenWindowFromURL(parent, GURL("about:blank"));
}

RenderFrameHostImpl* OpenWindowInitialEmptyDoc(RenderFrameHostImpl* parent) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation - none will commit.

  ShellAddedObserver observer;

  EXPECT_TRUE(ExecJs(parent, R"(
    window.open("/nocontent");
  )"));

  return GetMainFrameHostImpl(observer.GetShell());
}

RenderFrameHostImpl* OpenWindowFromJavascriptURL(RenderFrameHostImpl* parent) {
  // Note: We do not use OpenWindowFromURL() because we do not want to wait for
  // a navigation, since the `javascript:` URL will not commit (`about:blank`
  // will).

  ShellAddedObserver observer;

  EXPECT_TRUE(ExecJs(parent, R"(
    window.open("javascript:'foo'");
  )"));

  return GetMainFrameHostImpl(observer.GetShell());
}

RenderFrameHostImpl* OpenWindowFromBlob(RenderFrameHostImpl* parent) {
  GURL blob_url = CreateBlobURL(parent);
  return OpenWindowFromURL(parent, blob_url);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

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
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForAboutBlankFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

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
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForInitialEmptyDocFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForInitialEmptyDocFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForAboutSrcdocFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

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
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForAboutSrcdocFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForDataURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForDataURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       SandboxedIframeInheritsAddressSpaceForDataURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForJavascriptURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForJavascriptURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       SandboxedIframeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsAddressSpaceForBlobURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForFilesystemURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsAddressSpaceForFilesystemURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForFilesystemURLFromPublic) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsAddressSpaceForFilesystemURLFromLocal) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_TRUE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsSecureContextForAboutBlankFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromAboutBlank(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_FALSE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    IframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_TRUE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    OpeneeInheritsSecureContextForInitialEmptyDocFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowInitialEmptyDoc(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_FALSE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForAboutSrcdocFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromSrcdoc(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  // TODO(https://crbug.com/1168024): Expect true instead.
  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  // TODO(https://crbug.com/1168024): Expect true instead.
  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForDataURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromDataURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    IframeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddChildFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    OpeneeInheritsSecureContextForJavascriptURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_FALSE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsSecureContextForJavascriptURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromJavascriptURL(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_TRUE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromBlob(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_TRUE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       OpeneeInheritsSecureContextForBlobURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* window = OpenWindowFromBlob(root_frame_host());
  ASSERT_NE(nullptr, window);

  EXPECT_FALSE(window->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      window->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       IframeInheritsSecureContextForFilesystemURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    IframeInheritsSecureContextForFilesystemURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForFilesystemURLFromSecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_TRUE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_TRUE(security_state->is_web_secure_context);
}

IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    SandboxedIframeInheritsSecureContextForFilesystemURLFromInsecure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame =
      AddSandboxedChildFromFilesystem(root_frame_host());
  ASSERT_NE(nullptr, child_frame);

  EXPECT_FALSE(child_frame->is_web_secure_context());

  const network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_FALSE(security_state->is_web_secure_context);
}

// This test verifies that even with the blocking feature disabled, an insecure
// page in the `local` address space cannot fetch a `file:` URL.
//
// This is relevant to CORS-RFC1918, since `file:` URLs are considered `local`.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTestNoBlocking,
                       InsecurePageCannotRequestFile) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(
      false,
      EvalJs(root_frame_host(),
             FetchSubresourceScript(GetTestUrl("", "empty.html").spec())));
}

// This test verifies that even with the blocking feature disabled, a secure
// page in the `local` address space cannot fetch a `file:` URL.
//
// This is relevant to CORS-RFC1918, since `file:` URLs are considered `local`.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTestNoBlocking,
                       SecurePageCannotRequestFile) {
  EXPECT_TRUE(
      NavigateToURL(shell(), SecureDefaultURL(*embedded_test_server())));

  // Check that the page cannot load a `file:` URL.
  EXPECT_EQ(
      false,
      EvalJs(root_frame_host(),
             FetchSubresourceScript(GetTestUrl("", "empty.html").spec())));
}

// This test verifies that with the blocking feature disabled, the private
// network request policy used by RenderFrameHostImpl for requests is set
// to warn about insecure requests.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTestNoBlocking,
                       PrivateNetworkPolicyIsWarnByDefault) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::
                kWarnFromInsecureToMorePrivate);
}

// This test mimics the tests below, with the blocking feature disabled. It
// verifies that by default requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
//
// TODO(titouan): Make this pass. Request is currently blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTestNoBlocking,
                       PrivateNetworkRequestIsNotBlockedByDefault) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that by default, the private network request policy used
// by RenderFrameHostImpl for requests is set to block insecure requests.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       PrivateNetworkPolicyIsBlockByDefault) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::
                kBlockFromInsecureToMorePrivate);
}

// This test verifies that when the right feature is enabled but the content
// browser client overrides it, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    FromInsecureTreatAsPublicToLocalWithPolicySetToAllowIsNotBlocked) {
  GURL url = InsecureTreatAsPublicAddressURL(*embedded_test_server());

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kAllow);

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that child frames with distinct origins from their parent
// do not inherit their private network request policy, which is based on the
// origin of the child document instead.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       PrivateNetworkRequestPolicyCalculatedPerOrigin) {
  GURL url = InsecureTreatAsPublicAddressURL(*embedded_test_server());

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  RenderFrameHostImpl* child_frame = AddChildFromURL(
      root_frame_host(), SecureDefaultURL(*embedded_test_server()));

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::
                kBlockFromInsecureToMorePrivate);
}

// This test verifies that the initial empty document, which inherits its origin
// from the document creator, also inherits its private network request policy.
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    PrivateNetworkRequestPolicyInheritedWithOriginForInitialEmptyDoc) {
  GURL url = InsecureTreatAsPublicAddressURL(*embedded_test_server());

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

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
    CorsRfc1918BrowserTest,
    PrivateNetworkRequestPolicyInheritedWithOriginForAboutBlank) {
  GURL url = InsecureTreatAsPublicAddressURL(*embedded_test_server());

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

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
    CorsRfc1918BrowserTest,
    PrivateNetworkRequestPolicyNotInheritedWithOriginForDataURL) {
  GURL url = InsecureTreatAsPublicAddressURL(*embedded_test_server());

  PolicyTestContentBrowserClient client;
  client.SetAllowInsecurePrivateNetworkRequestsFrom(url::Origin::Create(url));

  // Register the client before we navigate, so that the navigation commits the
  // correct PrivateNetworkRequestPolicy.
  ContentBrowserClientRegistration registration(&client);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* child_frame = AddChildFromDataURL(root_frame_host());

  network::mojom::ClientSecurityStatePtr security_state =
      child_frame->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::
                kBlockFromInsecureToMorePrivate);
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       FromSecureTreatAsPublicToLocalIsNotBlocked) {
  EXPECT_TRUE(NavigateToURL(
      shell(), SecureTreatAsPublicAddressURL(*embedded_test_server())));

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       FromInsecureTreatAsPublicToLocalIsBlocked) {
  EXPECT_TRUE(NavigateToURL(
      shell(), InsecureTreatAsPublicAddressURL(*embedded_test_server())));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a public IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       FromInsecurePublicToLocalIsBlocked) {
  // Intercept the page load and pretend it came from a public IP.

  const GURL url = InsecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PublicAddress(), embedded_test_server()->port()));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a private IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       FromInsecurePrivateToLocalIsBlocked) {
  // Intercept the page load and pretend it came from a private IP.

  const GURL url = InsecureDefaultURL(*embedded_test_server());

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndPoint(
      url, net::IPEndPoint(PrivateAddress(), embedded_test_server()->port()));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a local IP address
//  - to local IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       FromInsecureLocalToLocalIsNotBlocked) {
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - embedded in an insecure page served from a local IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    CorsRfc1918BrowserTest,
    FromSecurePublicEmbeddedInInsecureLocalToLocalIsBlocked) {
  // First navigate to an insecure page served by a local IP address.
  EXPECT_TRUE(
      NavigateToURL(shell(), InsecureDefaultURL(*embedded_test_server())));

  // Then embed a secure public iframe.
  GURL iframe_url = SecureTreatAsPublicAddressURL(*embedded_test_server());
  std::string script = base::ReplaceStringPlaceholders(
      R"(
        const iframe = document.createElement("iframe");
        iframe.src = "$1";
        document.body.appendChild(iframe);
      )",
      {iframe_url.spec()}, nullptr);
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
  EXPECT_EQ(false, EvalJs(child_frame, FetchSubresourceScript("image.jpg")));
}

// This test verifies the initial values of a never committed
// RenderFrameHostImpl's ClientSecurityState.
IN_PROC_BROWSER_TEST_F(CorsRfc1918BrowserTest,
                       InitialNonCommittedRenderFrameHostClientSecurityState) {
  // Start a navigation. This forces the RenderFrameHost to initialize its
  // RenderFrame. The navigation is then cancelled by a HTTP 204 code.
  // We're left with a RenderFrameHost containing the default
  // ClientSecurityState values.
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(
      shell(), embedded_test_server()->GetURL("/nocontent")));

  auto client_security_state = root_frame_host()->BuildClientSecurityState();
  EXPECT_FALSE(client_security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            client_security_state->cross_origin_embedder_policy.value);
  EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
            client_security_state->ip_address_space);
  EXPECT_EQ(network::mojom::PrivateNetworkRequestPolicy::
                kBlockFromInsecureToMorePrivate,
            client_security_state->private_network_request_policy);
}

}  // namespace content
