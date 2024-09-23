// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLDocumentParserLoadingTest
    : public SimTest,
      public testing::WithParamInterface<ParserSynchronizationPolicy> {
 protected:
  HTMLDocumentParserLoadingTest() {
    Document::SetForceSynchronousParsingForTesting(GetParam() ==
                                                   kForceSynchronousParsing);
    platform_->SetAutoAdvanceNowToPendingTasks(false);
  }
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

INSTANTIATE_TEST_SUITE_P(HTMLDocumentParserLoadingTest,
                         HTMLDocumentParserLoadingTest,
                         testing::Values(kAllowDeferredParsing,
                                         kForceSynchronousParsing));

TEST_P(HTMLDocumentParserLoadingTest,
       PrefetchedDeferScriptDoesNotDeadlockParser) {
  // Maximum size string chunk to feed to the parser.
  constexpr unsigned kPumpSize = 2048;
  // <div>hello</div> is conveniently 16 chars in length.
  constexpr int kInitialDivCount = 1.5 * kPumpSize / 16;

  SimRequest::Params params;
  params.response_http_status = 200;

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest deferred_js("https://example.com/deferred-script.js",
                         "application/javascript", params);
  SimRequest sync_js("https://example.com/sync-script.js",
                     "application/javascript", params);
  LoadURL("https://example.com/test.html");
  // Building a big HTML document that the parser cannot handle in one go.
  // The idea is that we do
  //       PumpTokenizer PumpTokenizer  Insert PumpTokenizer PumpTokenizer ...
  // But _without_ calling Append, to replicate the deadlock situation
  // encountered in crbug.com/1132508. First, build some problematic input in a
  // StringBuilder.
  WTF::StringBuilder sb;
  sb.Append("<html>");
  sb.Append(R"HTML(
    <head>
        <meta charset="utf-8">
        <!-- Preload deferred-script.js so that a Client ends up backing
             the deferred_js SimRequest. -->
        <link rel="preload" href="deferred-script.js" as="script">
    </head><body>
  )HTML");
  for (int i = 0; i < kInitialDivCount; i++) {
    // Add a large blob of HTML to the parser to give it something to work with.
    // Must cross the first and second Append calls.
    sb.Append("<div>hello</div>");
  }
  // Next inject a synchronous, parser-blocking script and a div
  // for the defer script to work with.
  sb.Append(R"HTML(
    <script src="sync-script.js"></script>
    <div id="internalDiv"></div>
  )HTML");
  unsigned script_end = sb.length();
  for (int i = 0; i < kInitialDivCount; i++) {
    // Stress the parser more by requiring nested tokenization pumps.
    sb.Append("<script>document.write('hello');</script>");
  }
  // At the end of the document, add the deferred script.
  // When this runs, it'll add a worldDiv into the internalDiv created above.
  sb.Append(R"HTML(
    <script src="deferred-script.js" defer></script>
  )HTML");
  // Next, chop up the StringBuilder into realistic chunks.
  String s = sb.ToString();
  int testing_phase = 0;
  ASSERT_GT(s.length(), 1u);
  for (unsigned i = 0; i < s.length(); i += kPumpSize) {
    unsigned extent = kPumpSize - 1;
    if (i + extent > (s.length()) - 1) {
      extent = s.length() - 1 - i;
      ASSERT_LT(extent, kPumpSize);
    }
    String chunk(s.Characters8() + i, extent);
    main_resource.Write(chunk);
    if (i >= script_end) {
      // Simulate the deferred script arriving before the parser-blocking one.
      if (testing_phase == 1) {
        deferred_js.Complete(R"JS(
            document.getElementById("internalDiv").innerHTML = "<div id='worldDiv'>hi</div>";
          )JS");
      }
      testing_phase++;
      platform_->RunUntilIdle();
    }
  }
  // Everything's now Append()'d. Complete the main resource.
  ASSERT_GT(testing_phase, 2);
  main_resource.Complete();
  platform_->RunUntilIdle();  // Parse up until the parser blocking script.
  // Complete the parser blocking script.
  sync_js.Complete(R"JS(
    document.write("<div id='helloDiv'></div>");
  )JS");
  // Resume execution up until the parser-blocking script at the end.
  platform_->RunUntilIdle();
  // Expect both the element generated by the parser blocking script
  // and the element created by the deferred script to be present.
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("helloDiv")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("worldDiv")));
}

TEST_P(HTMLDocumentParserLoadingTest, IFrameDoesNotRenterParser) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest::Params params;
  params.response_http_status = 200;
  SimSubresourceRequest js("https://example.com/non-existent.js",
                           "application/javascript", params);
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
<script src="non-existent.js"></script>
<iframe onload="document.write('This test passes if it does not crash'); document.close();"></iframe>
  )HTML");
  platform_->RunUntilIdle();
  js.Complete("");
  platform_->RunUntilIdle();
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsInBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_head_resource("https://example.com/testHead.css",
                                          "text/css");
  SimSubresourceRequest css_body_resource("https://example.com/testBody.css",
                                          "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><head>
    <link rel=stylesheet href=testHead.css>
    </head><body>
    <div id="before"></div>
    <link rel=stylesheet href=testBody.css>
    <div id="after"></div>
    </body></html>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after")));

  // Completing the head css should progress parsing past #before.
  css_head_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after")));

  // Completing the body resource and pumping the tasks should continue parsing
  // and create the "after" div.
  css_body_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after")));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsInBodyIncremental) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_head_resource("https://example.com/testHead.css",
                                          "text/css");
  SimSubresourceRequest css_body_resource1("https://example.com/testBody1.css",
                                           "text/css");
  SimSubresourceRequest css_body_resource2("https://example.com/testBody2.css",
                                           "text/css");
  SimSubresourceRequest css_body_resource3("https://example.com/testBody3.css",
                                           "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <html><head>
    <link rel=stylesheet href=testHead.css>
    </head><body>
    <div id="before"></div>
    <link rel=stylesheet href=testBody1.css>
    <div id="after1"></div>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after3")));

  main_resource.Write(
      "<link rel=stylesheet href=testBody2.css>"
      "<div id=\"after2\"></div>");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after3")));

  main_resource.Complete(R"HTML(
    <link rel=stylesheet href=testBody3.css>
    <div id="after3"></div>
    </body></html>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after3")));

  // Completing the head css shouldn't change anything.
  css_head_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after3")));

  // Completing the second css shouldn't change anything
  css_body_resource2.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after3")));

  // Completing the first css should allow the parser to continue past it and
  // the second css which was already completed and then pause again before the
  // third css.
  css_body_resource1.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after3")));

  // Completing the third css should let it continue to the end.
  css_body_resource3.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after1")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after2")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after3")));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldNotPauseParsingForExternalNonMatchingStylesheetsInBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_head_resource("https://example.com/testHead.css",
                                          "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><head>
    <link rel=stylesheet href=testHead.css>
    </head><body>
    <div id="before"></div>
    <link rel=stylesheet href=testBody.css type='print'>
    <div id="after"></div>
    </body></html>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after")));

  css_head_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after")));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsImportedInBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_head_resource("https://example.com/testHead.css",
                                          "text/css");
  SimSubresourceRequest css_body_resource("https://example.com/testBody.css",
                                          "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><head>
    <link rel=stylesheet href=testHead.css>
    </head><body>
    <div id="before"></div>
    <style>
    @import 'testBody.css'
    </style>
    <div id="after"></div>
    </body></html>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after")));

  // Completing the head css should progress parsing past #before.
  css_head_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after")));

  // Completing the body resource and pumping the tasks should continue parsing
  // and create the "after" div.
  css_body_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after")));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsWrittenInBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_head_resource("https://example.com/testHead.css",
                                          "text/css");
  SimSubresourceRequest css_body_resource("https://example.com/testBody.css",
                                          "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><head>
    <link rel=stylesheet href=testHead.css>
    </head><body>
    <div id="before"></div>
    <script>
    document.write('<link rel=stylesheet href=testBody.css>');
    </script>
    <div id="after"></div>
    </body></html>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after")));

  // Completing the head css should progress parsing past #before.
  css_head_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_FALSE(GetDocument().getElementById(AtomicString("after")));

  // Completing the body resource and pumping the tasks should continue parsing
  // and create the "after" div.
  css_body_resource.Complete("");
  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after")));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldNotPauseParsingForExternalStylesheetsAttachedInBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_async_resource("https://example.com/testAsync.css",
                                           "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><head>
    </head><body>
    <div id="before"></div>
    <script>
    var attach  = document.getElementsByTagName('script')[0];
    var link  = document.createElement('link');
    link.rel  = 'stylesheet';
    link.type = 'text/css';
    link.href = 'testAsync.css';
    link.media = 'all';
    attach.appendChild(link);
    </script>
    <div id="after"></div>
    </body></html>
  )HTML");

  platform_->RunUntilIdle();
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("before")));
  EXPECT_TRUE(GetDocument().getElementById(AtomicString("after")));

  css_async_resource.Complete("");
  platform_->RunUntilIdle();
}

}  // namespace blink
