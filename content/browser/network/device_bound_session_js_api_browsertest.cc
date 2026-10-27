// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/unexportable_keys/features.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
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
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::Optional;

constexpr char kSessionId[] = "js_api_session";

// Used both in Set-Cookie and in the session's credentials.
constexpr std::string_view kCookieAttributes =
    "Domain=a.test; Path=/; Secure; SameSite=None";

constexpr char kNoBinderBadMessage[] =
    "No binder found for interface "
    "network.mojom.RestrictedDeviceBoundSessionManager for the frame/document "
    "scope";

// Resolves with the ids of the first batch of sessions the observer delivers.
// The observer only runs its callback for a non-empty batch, so this waits
// until a session is visible either in the snapshot or as a live push.
constexpr char kObserveSessionScript[] = R"(
  new Promise(resolve => {
    const observer = new DeviceBoundSessionRegistrationObserver(sessions => {
      observer.disconnect();
      resolve(sessions.map(s => s.sessionId).join(','));
    });
    observer.observe();
  })
)";

// Resolves with the refresh URL of the first session the observer delivers.
constexpr char kObserveRefreshUrlScript[] = R"(
  new Promise(resolve => {
    const observer = new DeviceBoundSessionRegistrationObserver(sessions => {
      observer.disconnect();
      resolve(sessions[0].refreshURL);
    });
    observer.observe();
  })
)";

// Resolves with "none" when no session is delivered within one second. If the
// snapshot reply is slower than that, a leaked session goes unnoticed.
constexpr char kObserveNothingScript[] = R"(
  new Promise(resolve => {
    const observer = new DeviceBoundSessionRegistrationObserver(sessions => {
      observer.disconnect();
      resolve('saw:' + sessions.map(s => s.sessionId).join(','));
    });
    observer.observe();
    setTimeout(() => {
      observer.disconnect();
      resolve('none');
    }, 1000);
  })
)";

constexpr char kAddIframeScript[] = R"(
  new Promise(resolve => {
    const iframe = document.createElement('iframe');
    iframe.src = $1;
    iframe.onload = () => resolve(true);
    document.body.appendChild(iframe);
  })
)";

std::unique_ptr<net::test_server::BasicHttpResponse> CreateHtmlResponse(
    std::string_view body) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(body);
  return response;
}

// Serves a site-wide session for a.test. a.test itself can register it. The
// .well-known file on a.test also lets accounts.a.test register for the site
// root. /dbsc_login?id=<id> registers a session with that id instead of
// kSessionId; the query goes on to registration and refresh.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    uint16_t port,
    const net::test_server::HttpRequest& request) {
  const std::string port_string = base::NumberToString(port);
  auto host_it = request.headers.find("Host");
  if (host_it == request.headers.end()) {
    return nullptr;
  }
  const std::string& host = host_it->second;
  const GURL url = request.GetURL();
  const std::string query =
      url.has_query() ? base::StrCat({"?", url.GetQuery()}) : std::string();

  if (url.path() == "/dbsc_login") {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        CreateHtmlResponse("<html><body>login</body></html>");
    response->AddCustomHeader(
        "Secure-Session-Registration",
        base::StrCat({"(RS256 ES256);challenge=\"challenge_value\";"
                      "path=\"dbsc_register",
                      query, "\""}));
    return response;
  }

  if (url.path() == "/dbsc_register" || url.path() == "/dbsc_refresh") {
    std::string session_id;
    if (!net::GetValueForKeyInQuery(url, "id", &session_id)) {
      session_id = kSessionId;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/json");
    response->AddCustomHeader(
        "Set-Cookie",
        base::StrCat({"auth_cookie=abcdef0123; ", kCookieAttributes}));
    const auto registration =
        base::DictValue()
            .Set("session_identifier", session_id)
            .Set("refresh_url", base::StrCat({"/dbsc_refresh", query}))
            .Set("scope", base::DictValue()
                              .Set("origin", base::StrCat({"https://a.test:",
                                                           port_string}))
                              .Set("include_site", true))
            .Set("credentials", base::ListValue().Append(
                                    base::DictValue()
                                        .Set("type", "cookie")
                                        .Set("name", "auth_cookie")
                                        .Set("attributes", kCookieAttributes)));
    response->set_content(*base::WriteJson(registration));
    return response;
  }

  if (request.relative_url == "/.well-known/device-bound-sessions" &&
      host == base::StrCat({"a.test:", port_string})) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/json");
    response->set_content(*base::WriteJson(base::DictValue().Set(
        "registering_origins",
        base::ListValue().Append(
            base::StrCat({"https://accounts.a.test:", port_string})))));
    return response;
  }

  if (request.relative_url == "/redirect_to_mail") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader(
        "Location",
        base::StrCat({"https://mail.a.test:", port_string, "/page"}));
    return response;
  }

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

class DeviceBoundSessionJsApiBrowserTestBase : public ContentBrowserTest {
 public:
  DeviceBoundSessionJsApiBrowserTestBase() {
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
    // The handler needs the port to build the scope origin and the
    // .well-known contents, so it is registered after listening starts.
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &HandleRequest, embedded_https_test_server().port()));
    embedded_https_test_server().StartAcceptingConnections();
  }

 protected:
  GURL GetUrl(std::string_view host, std::string_view path) {
    return embedded_https_test_server().GetURL(host, path);
  }

  // Registers an a.test session with `session_id`, then waits until a
  // top-level a.test page observes it alone in its first callback, so later
  // checks do not race the registration.
  void RegisterSession(Shell* window,
                       std::string_view session_id = kSessionId) {
    // The default session uses the plain login URL, so its refresh URL stays
    // /dbsc_refresh.
    const std::string login_path =
        session_id == kSessionId
            ? "/dbsc_login"
            : base::StrCat({"/dbsc_login?id=", session_id});
    ASSERT_TRUE(NavigateToURL(window, GetUrl("a.test", login_path)));
    ASSERT_TRUE(NavigateToURL(window, GetUrl("a.test", "/page")));
    ASSERT_EQ(EvalJs(window, kObserveSessionScript), session_id);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class DeviceBoundSessionJsApiBrowserTest
    : public DeviceBoundSessionJsApiBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBrowserTest,
                       ObserverSeesSessionAfterCrossSubdomainRedirect) {
  // accounts.a.test registers a session scoped to the site root.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("accounts.a.test", "/dbsc_login")));

  // The redirect lands on a different subdomain of the same site.
  ASSERT_TRUE(NavigateToURL(shell(),
                            GetUrl("accounts.a.test", "/redirect_to_mail"),
                            GetUrl("mail.a.test", "/page")));

  EXPECT_EQ(EvalJs(shell(), kObserveSessionScript), kSessionId);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBrowserTest,
                       CrossSiteIframeObservesNothing) {
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell()));

  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("b.test", "/page")));
  ASSERT_TRUE(
      ExecJs(shell(), JsReplace(kAddIframeScript, GetUrl("a.test", "/page"))));
  RenderFrameHost* child = ChildFrameAt(shell(), 0);
  ASSERT_TRUE(child);
  ASSERT_EQ(child->GetLastCommittedOrigin(),
            url::Origin::Create(GetUrl("a.test", "/")));

  // The same origin that saw the session at top level sees nothing when it is
  // embedded under another site.
  EXPECT_EQ(EvalJs(child, kObserveNothingScript), "none");
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBrowserTest,
                       IncognitoSessionIsolatedFromRegularProfile) {
  Shell* incognito = CreateOffTheRecordBrowser();
  ASSERT_NO_FATAL_FAILURE(RegisterSession(incognito, "incognito_session"));

  // A leaked Incognito session would already be stored, so it would be in the
  // regular profile's first callback. RegisterSession() checks that this
  // callback holds only the regular profile's own session.
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell(), "regular_session"));

  // A second Incognito window shares the off-the-record context, so it sees
  // that context's session and not the regular one.
  Shell* second_incognito = CreateOffTheRecordBrowser();
  ASSERT_EQ(incognito->web_contents()->GetBrowserContext(),
            second_incognito->web_contents()->GetBrowserContext());
  ASSERT_TRUE(NavigateToURL(second_incognito, GetUrl("a.test", "/page")));
  EXPECT_EQ(EvalJs(second_incognito, kObserveSessionScript),
            "incognito_session");
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBrowserTest,
                       CredentiallessIframeObservesNothing) {
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell()));
  ASSERT_TRUE(
      NavigateToURL(shell(), GetUrl("a.test", "/credentialless_parent")));
  RenderFrameHost* child = ChildFrameAt(shell(), 0);
  ASSERT_TRUE(child);
  ASSERT_TRUE(child->IsCredentialless());

  // Blink does not ask for the manager, so the renderer is not killed and the
  // observer sees nothing.
  EXPECT_EQ(EvalJs(child, kObserveNothingScript), "none");
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBrowserTest,
                       SameOriginIframeObservesSession) {
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell()));
  ASSERT_TRUE(
      ExecJs(shell(), JsReplace(kAddIframeScript, GetUrl("a.test", "/page"))));
  RenderFrameHost* child = ChildFrameAt(shell(), 0);
  ASSERT_TRUE(child);

  // The iframe commits from its initial empty document into the same window,
  // so this checks that a kept window gets the commit-time value.
  EXPECT_EQ(EvalJs(child, kObserveSessionScript), kSessionId);
}

IN_PROC_BROWSER_TEST_F(DeviceBoundSessionJsApiBrowserTest,
                       SiteDataBlockedAfterCommitObservesNothing) {
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell()));
  // A new document, so that its first observe() binds the manager after the
  // block below.
  ASSERT_TRUE(NavigateToURL(shell(), GetUrl("a.test", "/page")));
  auto* host = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(host->CanUseDeviceBoundSessionJsApi());

  // The user blocks cookies and site data for a.test while the page is open.
  std::vector<ContentSettingPatternSource> settings;
  settings.emplace_back(ContentSettingsPattern::FromString("[*.]a.test"),
                        ContentSettingsPattern::Wildcard(),
                        base::Value(CONTENT_SETTING_BLOCK),
                        content_settings::ProviderType::kNone,
                        /*incognito=*/false);
  base::test::TestFuture<void> settings_applied;
  shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::COOKIES, std::move(settings),
                           settings_applied.GetCallback());
  ASSERT_TRUE(settings_applied.Wait());

  // Cookie settings can change while the page is open, so they never cause a
  // kill. The page sees no session, and its renderer stays alive.
  EXPECT_EQ(EvalJs(shell(), kObserveNothingScript), "none");
  EXPECT_TRUE(host->IsRenderFrameLive());
}

class DeviceBoundSessionJsApiBinderBrowserTest
    : public DeviceBoundSessionJsApiBrowserTestBase {
 protected:
  RenderFrameHostImpl* main_frame() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

  // Binds the manager through a fresh broker pipe for `host`, the way a
  // renderer would, and returns the bad message reported while binding.
  // Only tests that expect a bad message use it: Blink never asks for the
  // manager from those documents, so a forced bind is the only way to reach
  // the browser's checks.
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
  // The iframe's observer gets the session only if its frame's broker bound
  // the manager with the frame's origin and isolation info.
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell()));
  ASSERT_TRUE(NavigateToURL(
      shell(),
      GetUrl("a.test", "/cross_site_iframe_factory.html?a.test(sub.a.test)")));
  RenderFrameHost* child = ChildFrameAt(shell(), 0);
  ASSERT_TRUE(child);
  EXPECT_EQ(EvalJs(child, kObserveSessionScript), kSessionId);
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
  // The page's observer gets the session only if its frame's broker bound
  // the manager with the frame's origin and first-party IsolationInfo.
  ASSERT_NO_FATAL_FAILURE(RegisterSession(shell()));
  EXPECT_EQ(EvalJs(shell(), kObserveRefreshUrlScript),
            GetUrl("a.test", "/dbsc_refresh").spec());
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
