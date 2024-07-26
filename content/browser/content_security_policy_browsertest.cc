// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_base.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using ContentSecurityPolicyBrowserTest = ContentBrowserTestBase;

// Test that the console error message for a Content Security Policy violation
// triggered by web assembly compilation does not mention the keyword
// 'wasm-eval' (which is currently only supported for extensions).  This is a
// regression test for https://crbug.com/1169592.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       WasmEvalBlockedConsoleMessage) {
  GURL url = embedded_test_server()->GetURL("/csp_wasm_eval.html");

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "[Report Only] Refused to compile or instantiate WebAssembly module "
      "because 'unsafe-eval' is not an allowed source of script in the "
      "following Content Security Policy directive: \"script-src "
      "'unsafe-inline'\".\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

// Test that creating a duplicate Trusted Types policy will yield a console
// message containing "already exists".
//
// This & the following test together ensure that different error causes get
// appropriate messages.
//
// Note: The bulk of Trusted Types related tests are found in the WPT suite
// under trusted-types/*. These two are here, because they need to access
// console messages.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       TrustedTypesCreatePolicyDupeMessage) {
  const char* page = R"(
      data:text/html,
      <meta http-equiv="Content-Security-Policy"
            content="require-trusted-types-for 'script';trusted-types a;">
      <script>
        trustedTypes.createPolicy("a", {});
        trustedTypes.createPolicy("a", {});
      </script>)";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*already exists*");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

// Test that creating a Trusted Types policy with a disallowed name will yield
// a console message indicating a directive has been violated.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       TrustedTypesCreatePolicyForbiddenMessage) {
  const char* page = R"(
      data:text/html,
      <meta http-equiv="Content-Security-Policy"
            content="require-trusted-types-for 'script';trusted-types a;">
      <script>
        trustedTypes.createPolicy("b", {});
      </script>)";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*violates*the following*directive*");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       WildcardNotMatchingNonNetworkSchemeBrowserSide) {
  const char* page = R"(
    data:text/html,
    <meta http-equiv="Content-Security-Policy" content="frame-src *">
    <iframe src="mailto:arthursonzogni@chromium.org"></iframe>
  )";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "Refused to frame '' because it violates the following Content Security "
      "Policy directive: \"frame-src *\". Note that '*' matches only URLs with "
      "network schemes ('http', 'https', 'ws', 'wss'), or URLs whose scheme "
      "matches `self`'s scheme. The scheme 'mailto:' must be added "
      "explicitly.\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       WildcardNotMatchingNonNetworkSchemeRendererSide) {
  const char* page = R"(
    data:text/html,
    <meta http-equiv="Content-Security-Policy" content="script-src *">
    <script src="mailto:arthursonzogni@chromium.org"></script>
  )";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "Refused to load the script 'mailto:arthursonzogni@chromium.org' because "
      "it violates the following Content Security Policy directive: "
      "\"script-src *\". Note that 'script-src-elem' was not explicitly set, "
      "so 'script-src' is used as a fallback. Note that '*' matches only URLs "
      "with network schemes ('http', 'https', 'ws', 'wss'), or URLs whose "
      "scheme matches `self`'s scheme. The scheme 'mailto:' must be added "
      "explicitly.\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

namespace {

base::FilePath TestFilePath(const char* filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return GetTestFilePath("", filename);
}

}  // namespace

// We test that we correctly match the file: scheme against file: URLs.
// Unfortunately, we cannot write this as Web Platform Test since Web Platform
// Tests don't support file: urls.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest, FileURLs) {
  GURL::Replacements add_localhost;
  add_localhost.SetHostStr("localhost");
  GURL::Replacements none;
  struct {
    const char* csp;
    std::string element_name;
    const raw_ref<const GURL::Replacements> document_host;
    const raw_ref<const GURL::Replacements> element_host;
    bool expect_allowed;
  } test_cases[] = {
      {"img-src 'none'", "img", raw_ref(none), raw_ref(none), false},
      {"img-src file:", "img", raw_ref(none), raw_ref(none), true},
      {"img-src 'self'", "img", raw_ref(none), raw_ref(none), true},
      {"img-src 'none'", "img", raw_ref(none), raw_ref(add_localhost), false},
      {"img-src file:", "img", raw_ref(none), raw_ref(add_localhost), true},
      {"img-src 'self'", "img", raw_ref(none), raw_ref(add_localhost), true},
      {"img-src 'none'", "img", raw_ref(add_localhost), raw_ref(none), false},
      {"img-src file:", "img", raw_ref(add_localhost), raw_ref(none), true},
      {"img-src 'self'", "img", raw_ref(add_localhost), raw_ref(none), true},
      {"img-src 'none'", "img", raw_ref(add_localhost), raw_ref(add_localhost),
       false},
      {"img-src file:", "img", raw_ref(add_localhost), raw_ref(add_localhost),
       true},
      {"img-src 'self'", "img", raw_ref(add_localhost), raw_ref(add_localhost),
       true},
      {"frame-src 'none'", "iframe", raw_ref(none), raw_ref(none), false},
      {"frame-src file:", "iframe", raw_ref(none), raw_ref(none), true},
      {"frame-src 'self'", "iframe", raw_ref(none), raw_ref(none), true},
      {"frame-src 'none'", "iframe", raw_ref(none), raw_ref(add_localhost),
       false},
      {"frame-src file:", "iframe", raw_ref(none), raw_ref(add_localhost),
       true},
      // TODO(antoniosartori): The following one behaves differently than
      // img-src.
      {"frame-src 'self'", "iframe", raw_ref(none), raw_ref(add_localhost),
       true},
      {"frame-src 'none'", "iframe", raw_ref(add_localhost), raw_ref(none),
       false},
      {"frame-src file:", "iframe", raw_ref(add_localhost), raw_ref(none),
       true},
      // TODO(antoniosartori): The following one behaves differently than
      // img-src.
      {"frame-src 'self'", "iframe", raw_ref(add_localhost), raw_ref(none),
       true},
      {"frame-src 'none'", "iframe", raw_ref(add_localhost),
       raw_ref(add_localhost), false},
      {"frame-src file:", "iframe", raw_ref(add_localhost),
       raw_ref(add_localhost), true},
      {"frame-src 'self'", "iframe", raw_ref(add_localhost),
       raw_ref(add_localhost), true},
  };

  for (const auto& test_case : test_cases) {
    GURL document_url = net::FilePathToFileURL(TestFilePath("hello.html"))
                            .ReplaceComponents(*test_case.document_host);

    // On windows, if `document_url` contains the host part "localhost", the
    // actual committed URL does not. So we omit EXPECT_TRUE and ignore the
    // result value here.
    std::ignore = NavigateToURL(shell(), document_url);

    GURL element_url = net::FilePathToFileURL(TestFilePath(
        test_case.element_name == "iframe" ? "empty.html" : "blank.jpg"));
    element_url = element_url.ReplaceComponents(*test_case.element_host);
    TestNavigationObserver load_observer(web_contents());

    EXPECT_TRUE(
        ExecJs(main_frame_host(),
               JsReplace(R"(
          var violation = new Promise(resolve => {
            document.addEventListener("securitypolicyviolation", (e) => {
              resolve("got violation");
            });
          });

          let meta = document.createElement('meta');
          meta.httpEquiv = 'Content-Security-Policy';
          meta.content = $1;
          document.head.appendChild(meta);

          let element = document.createElement($2);
          element.src = $3;
          var promise = new Promise(resolve => {
            element.onload = () => { resolve("allowed"); };
            element.onerror = () => { resolve("blocked"); };
          });
          document.body.appendChild(element);
    )",
                         test_case.csp, test_case.element_name, element_url)));

    if (test_case.element_name == "iframe") {
      // Since iframes always trigger the onload event, we need to be more
      // careful checking whether the iframe was blocked or not.
      load_observer.Wait();
      const url::Origin child_origin = main_frame_host()
                                           ->child_at(0)
                                           ->current_frame_host()
                                           ->GetLastCommittedOrigin();
      if (test_case.expect_allowed) {
        EXPECT_TRUE(load_observer.last_navigation_succeeded())
            << element_url << " in " << document_url << " with CSPs \""
            << test_case.csp << "\" should be allowed";
        EXPECT_FALSE(child_origin.opaque());
      } else {
        EXPECT_FALSE(load_observer.last_navigation_succeeded());
        EXPECT_EQ(net::ERR_BLOCKED_BY_CSP, load_observer.last_net_error_code());
        // The blocked frame's origin should become unique.
        EXPECT_TRUE(child_origin.opaque())
            << element_url << " in " << document_url << " with CSPs \""
            << test_case.csp << "\" should be blocked";
      }
    } else {
      std::string expect_message =
          test_case.expect_allowed ? "allowed" : "blocked";
      EXPECT_EQ(expect_message, EvalJs(main_frame_host(), "promise"))
          << element_url << " in " << document_url << " with CSPs \""
          << test_case.csp << "\" should be " << expect_message;
    }

    if (!test_case.expect_allowed) {
      EXPECT_EQ("got violation", EvalJs(main_frame_host(), "violation"));
    }
  }
}

// Test that a 'csp' attribute longer than 4096 bytes is ignored.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest, CSPAttributeTooLong) {
  std::string long_csp_attribute = "script-src 'none' ";
  long_csp_attribute.resize(4097, 'a');
  std::string page = "data:text/html,<body><iframe csp=\"" +
                     long_csp_attribute + "\"></iframe></body>";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("'csp' attribute too long*");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());

  EXPECT_EQ(main_frame_host()->child_count(), 1u);
  EXPECT_FALSE(main_frame_host()->child_at(0)->csp_attribute());
}

class TransparentPlaceholderImageContentSecurityPolicyBrowserTest
    : public ContentSecurityPolicyBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  TransparentPlaceholderImageContentSecurityPolicyBrowserTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          blink::features::kSimplifyLoadingTransparentPlaceholderImage);
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kSimplifyLoadingTransparentPlaceholderImage);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    TransparentPlaceholderImageContentSecurityPolicyBrowserTest,
    TransparentPlaceholderImageContentSecurityPolicyBrowserTest,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(
    TransparentPlaceholderImageContentSecurityPolicyBrowserTest,
    ImgSrcPolicyEnforced) {
  const char* page = R"(
    data:text/html,
    <meta http-equiv="Content-Security-Policy" content="img-src 'none';">
    <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==">
  )";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "Refused to load the image "
      "'data:image/gif;base64,R0lGODlhAQABAIAAAP///////"
      "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==' because it violates the following "
      "Content Security Policy directive: \"img-src 'none'\".\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_P(
    TransparentPlaceholderImageContentSecurityPolicyBrowserTest,
    ImgSrcPolicyReported) {
  GURL url = embedded_test_server()->GetURL("/csp_report_only_data_url.html");

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "[Report Only] Refused to load the image "
      "'data:image/gif;base64,R0lGODlhAQABAIAAAP///////"
      "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==' because it violates the following "
      "Content Security Policy directive: \"img-src 'none'\".\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(console_observer.Wait());
}

namespace {

constexpr char kWebmPath[] = "/csp_video.webm";

std::unique_ptr<net::test_server::HttpResponse> ServeCSPMedia(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kWebmPath) {
    return nullptr;
  }
  auto cookie_header = request.headers.find("cookie");
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (cookie_header == request.headers.end()) {
    response->set_code(net::HTTP_UNAUTHORIZED);
    return std::move(response);
  }
  response->set_code(net::HTTP_OK);
  const std::string kOneFrameOnePixelWebm =
      "GkXfo0AgQoaBAUL3gQFC8oEEQvOBCEKCQAR3ZWJtQoeBAkKFgQIYU4BnQN8VSalmQCgq17FA"
      "Aw9CQE2AQAZ3aGFtbXlXQUAGd2hhbW15RIlACECPQAAAAAAAFlSua0AxrkAu14EBY8WBAZyB"
      "ACK1nEADdW5khkAFVl9WUDglhohAA1ZQOIOBAeBABrCBlrqBlh9DtnVAdOeBAKNAboEAAIDy"
      "CACdASqWAJYAPk0ci0WD+IBAAJiWlu4XdQTSq2H4MW0+sMO0gz8HMRe+"
      "0jRo0aNGjRo0aNGjRo0aNGjRo0aNGjRo0aNGjRo0aNGjRo0VAAD+/729RWRzH4mOZ9/"
      "O8Dl319afX4gsgAAA";
  std::string content;
  base::Base64Decode(kOneFrameOnePixelWebm, &content);
  response->AddCustomHeader("Content-Security-Policy", "sandbox allow-scripts");
  response->AddCustomHeader("Content-Type", "video/webm");
  response->AddCustomHeader("Access-Control-Allow-Origin", "null");
  response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
  response->set_content(content);
  return std::move(response);
}

}  // namespace

class ThirdPartyCookiesContentSecurityPolicyBrowserTest
    : public ContentSecurityPolicyBrowserTest {
 public:
  ThirdPartyCookiesContentSecurityPolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(
        net::features::kForceThirdPartyCookieBlocking);
  }

  void SetUpOnMainThread() override {
    ContentSecurityPolicyBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->RegisterRequestHandler(base::BindRepeating(&ServeCSPMedia));
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentSecurityPolicyBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentSecurityPolicyBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    ContentSecurityPolicyBrowserTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that CSP does not break rendering access-controlled media due to
// third-party cookie blocking.
IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesContentSecurityPolicyBrowserTest,
                       CSPMediaThirdPartyCookieBlocking) {
  ASSERT_TRUE(content::SetCookie(web_contents()->GetBrowserContext(),
                                 https_server()->GetURL("/"),
                                 "foo=bar; SameSite=None; Secure;"));
  ASSERT_TRUE(NavigateToURL(shell(), https_server()->GetURL(kWebmPath)));
  EXPECT_TRUE(EvalJs(shell(),
                     "fetch('/csp_video.webm', {credentials: "
                     "'include'}).then(res => res.status == 200)")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiesContentSecurityPolicyBrowserTest,
                       CSPMediaThirdPartyCookieBlocking_IFrame) {
  ASSERT_TRUE(content::SetCookie(web_contents()->GetBrowserContext(),
                                 https_server()->GetURL("/"),
                                 "foo=bar; SameSite=None; Secure;"));
  std::string page = "data:text/html,<iframe src=\"" +
                     https_server()->GetURL(kWebmPath).spec() + "\"></iframe>";
  ASSERT_TRUE(NavigateToURL(shell(), GURL(page)));
  content::RenderFrameHost* nested_iframe = content::ChildFrameAt(shell(), 0);
  EXPECT_FALSE(EvalJs(nested_iframe,
                      "fetch('/csp_video.webm', {credentials: "
                      "'include'}).then(res => res.status == 200)")
                   .ExtractBool());
}

}  // namespace content
