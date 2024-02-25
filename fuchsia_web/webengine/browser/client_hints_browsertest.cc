// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"

namespace {

// Value returned by echoheader if header is not present in the request.
constexpr const char kHeaderNotPresent[] = "None";

// Client Hint header names defined by the spec.
constexpr const char kRoundTripTimeCH[] = "RTT";
constexpr const char kDeviceMemoryCH[] = "Sec-CH-Device-Memory";
constexpr const char kUserAgentCH[] = "Sec-CH-UA";
constexpr const char kFullVersionListCH[] = "Sec-CH-UA-Full-Version-List";
constexpr const char kArchCH[] = "Sec-CH-UA-Arch";
constexpr const char kBitnessCH[] = "Sec-CH-UA-Bitness";
constexpr const char kPlatformCH[] = "Sec-CH-UA-Platform";

// Expected Client Hint values that can be hardcoded.
constexpr const char k64Bitness[] = "\"64\"";
constexpr const char kFuchsiaPlatform[] = "\"Fuchsia\"";

// |str| is interpreted as a decimal number or integer.
void ExpectStringIsNonNegativeNumber(std::string& str) {
  double str_double;
  EXPECT_TRUE(base::StringToDouble(str, &str_double));
  EXPECT_GE(str_double, 0);
}

}  // namespace

class ClientHintsTest : public FrameImplTestBaseWithServer {
 public:
  ClientHintsTest() = default;
  ~ClientHintsTest() override = default;
  ClientHintsTest(const ClientHintsTest&) = delete;
  ClientHintsTest& operator=(const ClientHintsTest&) = delete;

  void SetUpOnMainThread() override {
    FrameImplTestBaseWithServer::SetUpOnMainThread();
    frame_for_test_ = FrameForTest::Create(context(), {});
  }

  void TearDownOnMainThread() override {
    frame_for_test_ = {};
    FrameImplTestBaseWithServer::TearDownOnMainThread();
  }

 protected:
  // Sets Client Hints for embedded test server to request from the content
  // embedder. Sends "Accept-CH" response header with |hint_types|, a
  // comma-separated list of Client Hint types.
  void SetClientHintsForTestServerToRequest(const std::string& hint_types) {
    GURL url = embedded_test_server()->GetURL(
        std::string("/set-header?Accept-CH: ") + hint_types);
    LoadUrlAndExpectResponse(frame_for_test_.GetNavigationController(), {},
                             url.spec());
    frame_for_test_.navigation_listener().RunUntilUrlEquals(url);
  }

  // Gets the value of |header| returned by WebEngine on a navigation.
  // Loads "/echoheader" which echoes the given |header|. The server responds to
  // this navigation request with the header value. Returns the header value,
  // which is read by JavaScript. Returns kHeaderNotPresent if header was not
  // sent during the request.
  std::string GetNavRequestHeaderValue(const std::string& header) {
    GURL url =
        embedded_test_server()->GetURL(std::string("/echoheader?") + header);
    LoadUrlAndExpectResponse(frame_for_test_.GetNavigationController(), {},
                             url.spec());
    frame_for_test_.navigation_listener().RunUntilUrlEquals(url);

    std::optional<base::Value> value =
        ExecuteJavaScript(frame_for_test_.get(), "document.body.innerText;");
    return value->GetString();
  }

  // Gets the value of |header| returned by WebEngine on a XMLHttpRequest.
  // Loads "/echoheader" which echoes the given |header|. The server responds to
  // the XMLHttpRequest with the header value, which is saved in a JavaScript
  // Promise. Returns the value of Promise, and returns kHeaderNotPresent if
  // header is not sent during the request. Requires a loaded page first. Call
  // TestServerRequestsClientHints or GetNavRequestHeaderValue first to have a
  // loaded page.
  std::string GetXHRRequestHeaderValue(const std::string& header) {
    constexpr char kScript[] = R"(
      new Promise(function (resolve, reject) {
        const xhr = new XMLHttpRequest();
        xhr.open("GET", "/echoheader?" + $1);
        xhr.onload = () => {
          resolve(xhr.response);
        };
        xhr.send();
      })
    )";
    FrameImpl* frame_impl =
        context_impl()->GetFrameImplForTest(&frame_for_test_.ptr());
    content::WebContents* web_contents = frame_impl->web_contents_for_test();
    return content::EvalJs(web_contents, content::JsReplace(kScript, header))
        .ExtractString();
  }

  // Fetches value of Client Hint |hint_type| for both navigation and
  // subresource requests, and calls |verify_callback| with the value.
  void GetAndVerifyClientHint(
      const std::string& hint_type,
      base::RepeatingCallback<void(std::string&)> verify_callback) {
    std::string result = GetNavRequestHeaderValue(hint_type);
    verify_callback.Run(result);
    result = GetXHRRequestHeaderValue(hint_type);
    verify_callback.Run(result);
  }

  FrameForTest frame_for_test_;
};

IN_PROC_BROWSER_TEST_F(ClientHintsTest, NumericalClientHints) {
  SetClientHintsForTestServerToRequest(std::string(kRoundTripTimeCH) + "," +
                                       std::string(kDeviceMemoryCH));
  GetAndVerifyClientHint(kRoundTripTimeCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest, InvalidClientHint) {
  // Check browser handles requests for an invalid Client Hint.
  SetClientHintsForTestServerToRequest("not-a-client-hint");
  GetAndVerifyClientHint("not-a-client-hint",
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

// Low-entropy User Agent Client Hints are sent by default without the origin
// needing to request them. For a list of low-entropy Client Hints, see
// https://wicg.github.io/client-hints-infrastructure/#low-entropy-hint-table/
IN_PROC_BROWSER_TEST_F(ClientHintsTest, LowEntropyClientHintsAreSentByDefault) {
  GetAndVerifyClientHint(
      kUserAgentCH, base::BindRepeating([](std::string& str) {
        EXPECT_TRUE(base::Contains(str, "Chromium"));
        EXPECT_TRUE(base::Contains(str, version_info::GetMajorVersionNumber()));
      }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest,
                       LowEntropyClientHintsAreSentWhenRequested) {
  SetClientHintsForTestServerToRequest(kUserAgentCH);
  GetAndVerifyClientHint(
      kUserAgentCH, base::BindRepeating([](std::string& str) {
        EXPECT_TRUE(base::Contains(str, "Chromium"));
        EXPECT_TRUE(base::Contains(str, version_info::GetMajorVersionNumber()));
      }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest,
                       HighEntropyClientHintsAreNotSentByDefault) {
  GetAndVerifyClientHint(kFullVersionListCH,
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest,
                       HighEntropyClientHintsAreSentWhenRequested) {
  SetClientHintsForTestServerToRequest(kFullVersionListCH);
  GetAndVerifyClientHint(
      kFullVersionListCH, base::BindRepeating([](std::string& str) {
        EXPECT_TRUE(base::Contains(str, "Chromium"));
        EXPECT_TRUE(base::Contains(str, version_info::GetVersionNumber()));
      }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest, ArchitectureIsArmOrX86) {
  SetClientHintsForTestServerToRequest(kArchCH);
  GetAndVerifyClientHint(kArchCH, base::BindRepeating([](std::string& str) {
#if defined(ARCH_CPU_X86_64)
                           EXPECT_EQ(str, "\"x86\"");
#elif defined(ARCH_CPU_ARM64)
                           EXPECT_EQ(str, "\"arm\"");
#else
#error Unsupported CPU architecture
#endif
                         }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest, BitnessIs64) {
  SetClientHintsForTestServerToRequest(kBitnessCH);
  GetAndVerifyClientHint(kBitnessCH, base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, k64Bitness);
                         }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest, PlatformIsFuchsia) {
  // Platform is a low-entropy Client Hint, so no need for test server to
  // request it.
  GetAndVerifyClientHint(kPlatformCH, base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kFuchsiaPlatform);
                         }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest, RemoveClientHint) {
  SetClientHintsForTestServerToRequest(std::string(kRoundTripTimeCH) + "," +
                                       std::string(kDeviceMemoryCH));
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // Remove device memory from list of requested Client Hints. Removed hints
  // should no longer be sent.
  SetClientHintsForTestServerToRequest(kRoundTripTimeCH);
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

IN_PROC_BROWSER_TEST_F(ClientHintsTest, AdditionalClientHintsAreAlwaysSent) {
  SetClientHintsForTestServerToRequest(kRoundTripTimeCH);

  // Enable device memory as an additional Client Hint.
  auto* client_hints_delegate =
      context_impl()->browser_context()->GetClientHintsControllerDelegate();
  client_hints_delegate->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});

  GetAndVerifyClientHint(kRoundTripTimeCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // The additional Client Hint is sent without needing to be requested.
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // Remove all additional Client Hints.
  client_hints_delegate->ClearAdditionalClientHints();

  // Request a different URL because the frame would not reload the page with
  // the same URL.
  GetAndVerifyClientHint(kRoundTripTimeCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // Removed additional Client Hint is no longer sent.
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

// The handling of ACCEPT-CH Frame feature of client hints reliability can cause
// a Restart in the navigation stack. This has caused infinite internal
// redirects in the past when there is a URL request rewrite rule registered.
// This test makes sure the two do not break each other. See crbug.com/1356277
// for context.
IN_PROC_BROWSER_TEST_F(ClientHintsTest, WithUrlRedirectRules) {
  net::EmbeddedTestServer http2_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2);

  http2_server.ServeFilesFromSourceDirectory(kTestServerRoot);
  http2_server.SetAlpsAcceptCH(
      /*hostname=*/"", base::JoinString({kBitnessCH, kPlatformCH}, ","));
  http2_server.RegisterRequestMonitor(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        EXPECT_TRUE(request.headers.contains(kBitnessCH));
        EXPECT_EQ(request.headers.at(kBitnessCH), k64Bitness);
        EXPECT_TRUE(request.headers.contains(kPlatformCH));
        EXPECT_EQ(request.headers.at(kPlatformCH), kFuchsiaPlatform);
      }));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle = http2_server.StartAndReturnHandle());

  fuchsia::web::UrlRequestRewriteAppendToQuery append_to_query;
  append_to_query.set_query("foo=1&bar=2");

  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_append_to_query(std::move(append_to_query));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_hosts_filter({http2_server.base_url().host()});
  rule.set_schemes_filter({http2_server.base_url().scheme()});
  rule.mutable_rewrites()->push_back(std::move(rewrite));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));

  base::RunLoop run_loop;
  frame_for_test_->SetUrlRequestRewriteRules(
      std::move(rules), [&run_loop]() { run_loop.Quit(); });
  run_loop.Run();

  GURL url = http2_server.GetURL("/title1.html");
  EXPECT_TRUE(LoadUrlAndExpectResponse(
      frame_for_test_.GetNavigationController(), {}, url.spec()));
  frame_for_test_.navigation_listener().RunUntilLoaded();
  EXPECT_EQ(frame_for_test_.navigation_listener().current_state()->url(),
            url.spec() + "?foo=1&bar=2");
}

// Used as a HandleRequestCallback for EmbeddedTestServer to test Client Hint
// behavior in a sandboxed page. Defines two endpoints:
//
//   - /set sends back a response with `Accept-CH` header set as
//   `client_hint_type`.
//   - /get sends back a response body with the value of the `client_hint_type`
//   header from the request.
std::unique_ptr<net::test_server::HttpResponse>
SandboxedClientHintsRequestHandler(
    const std::string client_hint_type,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->AddCustomHeader("Content-Security-Policy", "sandbox allow-scripts");

  if (request.relative_url == "/set") {
    response->AddCustomHeader("Accept-CH", client_hint_type);
  } else if (request.relative_url == "/get") {
    auto it = request.headers.find(client_hint_type);
    if (it != request.headers.end()) {
      response->set_content(it->second);
    }
  } else {
    return nullptr;
  }
  return std::move(response);
}

// Ensure that client hints can be fetched from pages where the origin is
// opaque. This has caused crashes in the past, see crbug.com/1337431 for
// context.
IN_PROC_BROWSER_TEST_F(ClientHintsTest, HintsAreSentFromSandboxedPage) {
  net::EmbeddedTestServer http_server;
  http_server.RegisterRequestHandler(
      base::BindRepeating(&SandboxedClientHintsRequestHandler, kBitnessCH));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle = http_server.StartAndReturnHandle());

  GURL url = http_server.GetURL("/set");
  LoadUrlAndExpectResponse(frame_for_test_.GetNavigationController(), {},
                           url.spec());
  frame_for_test_.navigation_listener().RunUntilUrlEquals(url);

  url = http_server.GetURL("/get");
  LoadUrlAndExpectResponse(frame_for_test_.GetNavigationController(), {},
                           url.spec());
  frame_for_test_.navigation_listener().RunUntilUrlEquals(url);

  std::optional<base::Value> value =
      ExecuteJavaScript(frame_for_test_.get(), "document.body.innerText;");
  EXPECT_EQ(value->GetString(), k64Bitness);
}
