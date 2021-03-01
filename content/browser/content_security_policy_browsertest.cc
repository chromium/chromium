// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class ContentSecurityPolicyBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

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
  console_observer.Wait();
}

}  // namespace content
