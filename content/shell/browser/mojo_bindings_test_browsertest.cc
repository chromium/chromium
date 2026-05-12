// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

constexpr char kMojoHtml[] = R"(
    <script src="/mojo/public/js/mojo_bindings_lite.js"></script>
    <script src="/url/mojom/url.mojom-lite.js"></script>
    <script src="/url/mojom/scheme_host_port.mojom-lite.js"></script>
)";

std::unique_ptr<net::test_server::HttpResponse> HandleMojoRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/mojo.html") {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(kMojoHtml);
  return response;
}

class MojoBindingsTestBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MojoJS,MojoJSTest");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    base::FilePath gen_dir;
    CHECK(base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_dir));

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleMojoRequest));
    embedded_test_server()->ServeFilesFromDirectory(gen_dir);
    embedded_test_server()->ServeFilesFromDirectory(
        GetTestFilePath(nullptr, ""));
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

#if defined(MOJOM_FUZZER_BUILD_MODE)
IN_PROC_BROWSER_TEST_F(MojoBindingsTestBrowserTest,
                       InvokeMojoConstructorWithArgs) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/mojo.html")));

  EXPECT_EQ(
      "example.com",
      EvalJs(
          shell(),
          "new url.mojom.SchemeHostPort('https', 'example.com', 8080).host"));
}
#else
IN_PROC_BROWSER_TEST_F(MojoBindingsTestBrowserTest,
                       InvokeMojoEmptyConstructor) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/mojo.html")));

  EXPECT_EQ(base::Value(),
            EvalJs(shell(), "new url.mojom.SchemeHostPort().host"));
  EXPECT_EQ(
      base::Value(),
      EvalJs(
          shell(),
          "new url.mojom.SchemeHostPort('https', 'example.com', 8080).host"));
}
#endif

}  // namespace content
