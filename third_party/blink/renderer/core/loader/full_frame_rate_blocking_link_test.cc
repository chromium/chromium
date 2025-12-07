// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FullFrameRateBlockingLinkTest : public SimTest {
 protected:
  bool IsDocumentBlockedForFullFrameRate() {
    return GetDocument().HasFullFrameRateBlockingExpectLinkElements();
  }
};

TEST_F(FullFrameRateBlockingLinkTest, OriginallyNotBlocked) {
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
}

TEST_F(FullFrameRateBlockingLinkTest, BlockedWithValidElementLink) {
  // Test that full frame rate is blocked with valid blocking="full-frame-rate"
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="expect" href="#target-id" blocking="full-frame-rate"/>
  )HTML");
  EXPECT_TRUE(IsDocumentBlockedForFullFrameRate());
  main_resource.Complete("</head><body>some text</body>");
}

TEST_F(FullFrameRateBlockingLinkTest, NotBlockedWithNonExpectRel) {
  // Test that full frame rate is not blocked with non-expect rel
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest css_resource("https://example.com/test.css", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="stylesheet" href="test.css"  blocking="full-frame-rate"/>
  )HTML");
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
  css_resource.Finish();
  main_resource.Complete("</head><body>some text</body>");
}

TEST_F(FullFrameRateBlockingLinkTest, NotBlockedWithEmptyHrefId) {
  // Test that full frame rate is not blocked if href is empty
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="expect" href="" blocking="full-frame-rate"/>
  )HTML");
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
  main_resource.Complete("</head><body>some text</body>");
}

TEST_F(FullFrameRateBlockingLinkTest, NotBlockedForRenderBlockingAttribute) {
  // Test that full frame rate is not blocked if blocking attribute is render
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="expect" href="#target-id" blocking="render"/>
  )HTML");
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
  main_resource.Complete("</head><body>some text</body>");
}

TEST_F(FullFrameRateBlockingLinkTest, NotBlockedForInvalidBlockingAttribute) {
  // Test that full frame rate is not blocked if blocking attribute is invalid
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="expect" href="#target-id" blocking="invalid-value"/>
  )HTML");
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
  main_resource.Complete("</head><body>some text</body>");
}

TEST_F(FullFrameRateBlockingLinkTest, UnblockedWhenElementLinkParsed) {
  // Test that full frame rate is unblocked after an element with target-id
  // is parsed.
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="expect" href="#target-id" blocking="full-frame-rate"/>
  )HTML");
  EXPECT_TRUE(IsDocumentBlockedForFullFrameRate());
  main_resource.Write(R"HTML(
    </head>
    <body>
      <div id="target-id"></div>
  )HTML");
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
  main_resource.Complete("</body>");
}

TEST_F(FullFrameRateBlockingLinkTest, UnblockedWhenDocumentCompletes) {
  // Test that full frame rate is unblocked when the document completes
  // even if the target element is not parsed.
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="expect" href="#target-id" blocking="full-frame-rate"/>
  )HTML");
  EXPECT_TRUE(IsDocumentBlockedForFullFrameRate());
  main_resource.Complete("</head><body>some text</body>");
  EXPECT_FALSE(IsDocumentBlockedForFullFrameRate());
}

}  // namespace blink
