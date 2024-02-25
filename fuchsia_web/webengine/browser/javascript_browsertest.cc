// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/mem_buffer_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage1Title[] = "title 1";

constexpr char kDynamicTitlePath[] = "/dynamic_title.html";
constexpr int64_t kOnLoadScriptId = 0;

using JavaScriptTest = FrameImplTestBase;

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScript) {
  constexpr int64_t kBindingsId = 1234;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(url);
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptUpdated) {
  constexpr int64_t kBindingsId = 1234;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Verify that this script clobbers the previous script, as opposed to being
  // injected alongside it. (The latter would result in the title being
  // "helloclobber").
  frame->AddBeforeLoadJavaScript(
      kBindingsId, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = document.title + 'clobber';",
                                "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "clobber");
}

// Verifies that bindings are injected in order by producing a cumulative,
// non-commutative result.
IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptOrdered) {
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId1, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });
  frame->AddBeforeLoadJavaScript(
      kBindingsId2, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title += ' there';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello there");
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptRemoved) {
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId1, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'foo';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Add a script which clobbers "foo".
  frame->AddBeforeLoadJavaScript(
      kBindingsId2, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'bar';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Deletes the clobbering script.
  frame->RemoveBeforeLoadJavaScript(kBindingsId2);

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "foo");
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptRemoveInvalidId) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));
  auto frame = FrameForTest::Create(context(), {});

  frame->RemoveBeforeLoadJavaScript(kOnLoadScriptId);

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);
}

// Test JS injection using ExecuteJavaScriptNoResult() to set a value, and
// ExecuteJavaScript() to retrieve that value.
IN_PROC_BROWSER_TEST_F(JavaScriptTest, ExecuteJavaScript) {
  constexpr char kJsonStringLiteral[] = "\"I am a literal, literally\"";
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  const GURL kUrl(embedded_test_server()->GetURL(kPage1Path));

  // Navigate to a page and wait for it to finish loading.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, kPage1Title);

  // Execute with no result to set the variable.
  frame->ExecuteJavaScriptNoResult(
      {kUrl.DeprecatedGetOriginAsURL().spec()},
      base::MemBufferFromString(
          base::StringPrintf("my_variable = %s;", kJsonStringLiteral), "test"),
      [](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Execute a script snippet to return the variable's value.
  base::RunLoop loop;
  frame->ExecuteJavaScript(
      {kUrl.DeprecatedGetOriginAsURL().spec()},
      base::MemBufferFromString("my_variable;", "test"),
      [&](fuchsia::web::Frame_ExecuteJavaScript_Result result) {
        ASSERT_TRUE(result.is_response());
        std::string result_json =
            *base::StringFromMemBuffer(result.response().result);
        EXPECT_EQ(result_json, kJsonStringLiteral);
        loop.Quit();
      });
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptVmoDestroyed) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello");
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptWrongOrigin) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {"http://example.com"},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Expect that the original HTML title is used, because we didn't inject a
  // script with a replacement title.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      url, "Welcome to Stan the Offline Dino's Homepage");
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BeforeLoadScriptWildcardOrigin) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {"*"},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Test script injection for the origin 127.0.0.1.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello");

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  // Test script injection using a different origin ("localhost"), which should
  // still be picked up by the wildcard.
  GURL alt_url = embedded_test_server()->GetURL("localhost", kDynamicTitlePath);
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       alt_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(alt_url, "hello");
}

// Test that we can inject scripts before and after RenderFrame creation.
IN_PROC_BROWSER_TEST_F(JavaScriptTest,
                       BeforeLoadScriptEarlyAndLateRegistrations) {
  constexpr int64_t kOnLoadScriptId2 = kOnLoadScriptId + 1;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello");

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId2, {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("stashed_title += ' there';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Navigate away to clean the slate.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  // Navigate back and see if both scripts are working.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello there");
}

IN_PROC_BROWSER_TEST_F(JavaScriptTest, BadEncoding) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  base::RunLoop run_loop;

  // 0xFE is an illegal UTF-8 byte; it should cause UTF-8 conversion to fail.
  frame->ExecuteJavaScriptNoResult(
      {embedded_test_server()->GetOrigin().Serialize()},
      base::MemBufferFromString("true;\xfe", "test"),
      [&run_loop](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        EXPECT_TRUE(result.is_err());
        EXPECT_EQ(result.err(), fuchsia::web::FrameError::BUFFER_NOT_UTF8);
        run_loop.Quit();
      });
  run_loop.Run();
}

}  // namespace
