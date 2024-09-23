// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/common/test/url_request_rewrite_test_util.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test harness for browser tests exercising the URL request rewrite rule
// invalidation logic. Sending incorrect rules should result in the frame
// disconnecting.
class UrlRequestInvalidationTest : public WebEngineBrowserTest {
 public:
  UrlRequestInvalidationTest() = default;
  ~UrlRequestInvalidationTest() override = default;

  UrlRequestInvalidationTest(const UrlRequestInvalidationTest&) = delete;
  UrlRequestInvalidationTest& operator=(const UrlRequestInvalidationTest&) =
      delete;

  // Tests that sending |rewrite| will cause the fuchsia.web.Frame to disconnect
  // with ZX_ERR_INVALID_ARGS.
  void ExpectInvalidArgsForRewrite(fuchsia::web::UrlRequestRewrite rewrite) {
    auto frame = FrameForTest::Create(context(), {});
    base::RunLoop run_loop;
    frame.ptr().set_error_handler([&run_loop](zx_status_t status) {
      EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
      run_loop.Quit();
    });

    std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
    rewrites.push_back(std::move(rewrite));
    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));

    frame->SetUrlRequestRewriteRules(std::move(rules), []() {});
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, EmptyRewrite) {
  ExpectInvalidArgsForRewrite(fuchsia::web::UrlRequestRewrite());
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, InvalidAddHeaderName) {
  ExpectInvalidArgsForRewrite(CreateRewriteAddHeaders("Te\nst1", "Value"));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, InvalidAddHeaderValue) {
  ExpectInvalidArgsForRewrite(CreateRewriteAddHeaders("Test1", "Val\nue"));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, EmptyAddHeader) {
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_add_headers(fuchsia::web::UrlRequestRewriteAddHeaders());
  ExpectInvalidArgsForRewrite(std::move(rewrite));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, InvalidRemoveHeaderName) {
  ExpectInvalidArgsForRewrite(CreateRewriteRemoveHeader("Query", "Head\ner"));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, EmptyRemoveHeader) {
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_remove_header(fuchsia::web::UrlRequestRewriteRemoveHeader());
  ExpectInvalidArgsForRewrite(std::move(rewrite));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest,
                       EmptySubstituteQueryPattern) {
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_substitute_query_pattern(
      fuchsia::web::UrlRequestRewriteSubstituteQueryPattern());
  ExpectInvalidArgsForRewrite(std::move(rewrite));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest,
                       InvalidReplaceUrlNewUrlWithNullUrlPath) {
  ExpectInvalidArgsForRewrite(
      CreateRewriteReplaceUrl("some%00thing", "http:site:xyz"));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, InvalidReplaceUrlNewUrl) {
  ExpectInvalidArgsForRewrite(
      CreateRewriteReplaceUrl("/something", "http:site:xyz"));
}

// TODO(http://crbug.com/4596360): The following cases should succeed, but
// checking for success requires non-trivial work since this
// `UrlRequestInvalidationTest` is designed to test only invalid cases. Someone
// familiar with this code might want to test success cases.
//
// - CreateRewriteReplaceUrl("/something", "http://site.xyz/"));
// - CreateRewriteReplaceUrl("some%00thing", "http://site.xyz/"));
// - CreateRewriteReplaceUrl("/something", "http://site.xyz/%00"));
// - CreateRewriteReplaceUrl("some%00thing", "http://site.xyz/%00"));

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, EmptyReplaceUrl) {
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_replace_url(fuchsia::web::UrlRequestRewriteReplaceUrl());
  ExpectInvalidArgsForRewrite(std::move(rewrite));
}

IN_PROC_BROWSER_TEST_F(UrlRequestInvalidationTest, EmptyAppendToQuery) {
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_append_to_query(fuchsia::web::UrlRequestRewriteAppendToQuery());
  ExpectInvalidArgsForRewrite(std::move(rewrite));
}

}  // namespace
