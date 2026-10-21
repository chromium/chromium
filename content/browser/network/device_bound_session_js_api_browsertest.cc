// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/unexportable_keys/features.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_info.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::Optional;

constexpr char kNoBinderBadMessage[] =
    "No binder found for interface "
    "network.mojom.RestrictedDeviceBoundSessionManager for the frame/document "
    "scope";

std::unique_ptr<net::test_server::BasicHttpResponse> CreateHtmlResponse(
    std::string_view body) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(body);
  return response;
}

// Serves the pages that the binder tests load.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/page") {
    return CreateHtmlResponse("<html><body>page</body></html>");
  }

  if (request.relative_url == "/sandboxed_page") {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        CreateHtmlResponse("<html><body>sandboxed</body></html>");
    response->AddCustomHeader("Content-Security-Policy",
                              "sandbox allow-scripts");
    return response;
  }

  if (request.relative_url == "/credentialless_parent") {
    return CreateHtmlResponse(
        "<html><body><iframe credentialless src=\"/page\"></iframe>"
        "</body></html>");
  }

  return nullptr;
}

class DeviceBoundSessionJsApiBinderBrowserTest : public ContentBrowserTest {
 public:
  DeviceBoundSessionJsApiBinderBrowserTest() {
    // The SessionService creates its keys in the network service, which runs
    // in its own process; software keys let it do so on every platform.
    feature_list_.InitWithFeatures(
        {features::kDeviceBoundSessionJsApi,
         net::features::kDeviceBoundSessions,
         unexportable_keys::
             kEnableBoundSessionCredentialsSoftwareKeysForManualTesting},
        {});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
    embedded_https_test_server().RegisterRequestHandler(
        base::BindRepeating(&HandleRequest));
    embedded_https_test_server().StartAcceptingConnections();
  }

 protected:
  GURL GetUrl(std::string_view host, std::string_view path) {
    return embedded_https_test_server().GetURL(host, path);
  }

  RenderFrameHostImpl* main_frame() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

  // Binds the manager through a fresh broker pipe for `host`, the way a
  // renderer would, and returns the bad message reported while binding.
  //
  // TODO(crbug.com/494489935): Bind through the JS API in the tests that
  // expect a binding, and check the result in script.
  std::optional<std::string> BindManager(
      RenderFrameHostImpl* host,
      mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager>&
          manager) {
    std::optional<std::string> bad_message;
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          EXPECT_EQ(bad_message, std::nullopt);
          bad_message = error;
        }));

    EXPECT_TRUE(host->ResetBrowserInterfaceBrokerReceiverForTesting());
    mojo::Remote<blink::mojom::BrowserInterfaceBroker> broker;
    host->BindBrowserInterfaceBrokerReceiver(
        broker.BindNewPipeAndPassReceiver());
    broker->GetInterface(manager.BindNewPipeAndPassReceiver());
    broker.FlushForTesting();
    // A dropped receiver shows up as a disconnect once the pipe is flushed.
    manager.FlushForTesting();

    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    return bad_message;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       OpaqueOriginKillsRenderer) {
  // A data: document has an opaque origin with no precursor and runs in a
  // regular renderer process.
  ASSERT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<p>opaque</p>")));
  RenderFrameHostImpl* host = main_frame();
  ASSERT_TRUE(host->GetLastCommittedOrigin().opaque());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(host->GetProcess());
  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(host, manager), std::nullopt);
  EXPECT_EQ(bad_message::RFH_DEVICE_BOUND_SESSION_JS_API_NOT_AVAILABLE,
            kill_waiter.Wait());
  EXPECT_FALSE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       SandboxedOpaqueOriginKillsRenderer) {
  // A CSP-sandboxed document runs in a sandboxed process, which also fails
  // the ChildProcessSecurityPolicy check. Its opaque origin makes the
  // commit-time value false, and that check runs first.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("a.test", "/sandboxed_page")));
  RenderFrameHostImpl* host = main_frame();
  ASSERT_TRUE(host->GetLastCommittedOrigin().opaque());
  ASSERT_TRUE(host->GetSiteInstance()->GetSiteInfo().IsSandboxed());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(host->GetProcess());
  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(host, manager), std::nullopt);
  EXPECT_EQ(bad_message::RFH_DEVICE_BOUND_SESSION_JS_API_NOT_AVAILABLE,
            kill_waiter.Wait());
  EXPECT_FALSE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       CredentiallessIframeKillsRenderer) {
  ASSERT_TRUE(
      NavigateToURL(shell(), GetUrl("a.test", "/credentialless_parent")));
  auto* child =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame(), 0));
  ASSERT_TRUE(child);
  ASSERT_TRUE(child->IsCredentialless());
  ASSERT_TRUE(child->GetIsolationInfoForSubresources().nonce().has_value());
  ASSERT_FALSE(child->CanUseDeviceBoundSessionJsApi());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(child->GetProcess());
  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(child, manager), std::nullopt);
  EXPECT_EQ(bad_message::RFH_DEVICE_BOUND_SESSION_JS_API_NOT_AVAILABLE,
            kill_waiter.Wait());
  EXPECT_FALSE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       CrossSiteIframeKillsRenderer) {
  ASSERT_TRUE(NavigateToURL(
      shell(),
      GetUrl("b.test", "/cross_site_iframe_factory.html?b.test(a.test)")));
  auto* child =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame(), 0));
  ASSERT_TRUE(child);
  ASSERT_EQ(child->GetLastCommittedOrigin(),
            url::Origin::Create(GetUrl("a.test", "/")));
  ASSERT_FALSE(child->CanUseDeviceBoundSessionJsApi());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(child->GetProcess());
  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(child, manager), std::nullopt);
  EXPECT_EQ(bad_message::RFH_DEVICE_BOUND_SESSION_JS_API_NOT_AVAILABLE,
            kill_waiter.Wait());
  EXPECT_FALSE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       SameSiteIframeBindsManager) {
  ASSERT_TRUE(NavigateToURL(
      shell(),
      GetUrl("a.test", "/cross_site_iframe_factory.html?a.test(sub.a.test)")));
  auto* child =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame(), 0));
  ASSERT_TRUE(child);
  ASSERT_TRUE(child->CanUseDeviceBoundSessionJsApi());

  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(child, manager), std::nullopt);
  EXPECT_TRUE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       PdfProcessKillsRenderer) {
  // Commits the page as PDF content, so it runs in a PDF process, which may
  // not access data for any origin. The commit-time value is false, so a
  // forced bind is a bad message.
  ASSERT_TRUE(
      NavigateToURLWithPdf(shell()->web_contents(), GetUrl("a.test", "/page")));
  RenderFrameHostImpl* host = main_frame();
  ASSERT_TRUE(host->GetSiteInstance()->GetSiteInfo().is_pdf());
  ASSERT_FALSE(host->CanUseDeviceBoundSessionJsApi());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(host->GetProcess());
  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(host, manager), std::nullopt);
  EXPECT_EQ(bad_message::RFH_DEVICE_BOUND_SESSION_JS_API_NOT_AVAILABLE,
            kill_waiter.Wait());
  EXPECT_FALSE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       HttpPageKillsRenderer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  RenderFrameHostImpl* host = main_frame();
  ASSERT_FALSE(host->CanUseDeviceBoundSessionJsApi());

  RenderProcessHostBadIpcMessageWaiter kill_waiter(host->GetProcess());
  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(host, manager), std::nullopt);
  EXPECT_EQ(bad_message::RFH_DEVICE_BOUND_SESSION_JS_API_NOT_AVAILABLE,
            kill_waiter.Wait());
  EXPECT_FALSE(manager.is_connected());
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderBrowserTest,
                       HttpsPageBindsManager) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("a.test", "/page")));

  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_EQ(BindManager(main_frame(), manager), std::nullopt);
  EXPECT_TRUE(manager.is_connected());
}

// Turns DeviceBoundSessionJsApi back off, its default, so the broker has no
// binder for the manager.
class DeviceBoundSessionJsApiBinderFeatureOffBrowserTest
    : public DeviceBoundSessionJsApiBinderBrowserTest {
 public:
  DeviceBoundSessionJsApiBinderFeatureOffBrowserTest() {
    feature_off_list_.InitAndDisableFeature(features::kDeviceBoundSessionJsApi);
  }

 private:
  base::test::ScopedFeatureList feature_off_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBinderFeatureOffBrowserTest,
                       ReportsNoBinder) {
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("a.test", "/page")));

  mojo::Remote<network::mojom::RestrictedDeviceBoundSessionManager> manager;
  EXPECT_THAT(BindManager(main_frame(), manager),
              Optional(std::string(kNoBinderBadMessage)));
  EXPECT_FALSE(manager.is_connected());
}

}  // namespace

}  // namespace content
