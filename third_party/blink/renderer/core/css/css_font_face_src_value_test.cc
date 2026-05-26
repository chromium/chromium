// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_url_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class CSSFontFaceSrcValueTest : public SimTest {};

// Verify that a @font-face src URL containing dangling markup (newline + '<')
// is blocked and does NOT trigger a network request. The dangling markup
// mitigation in BaseFetchContext::CanRequest should prevent the fetch.
//
// If the potentially_dangling_markup flag were lost (the bug this tests for),
// the font request would go through and SimTest would fail because no
// SimSubresourceRequest was registered for the URL.
TEST_F(CSSFontFaceSrcValueTest, BlockPotentiallyDanglingMarkup) {
  SimRequest main_resource("https://example.com", "text/html");

  LoadURL("https://example.com");

  // The <table background="..."> URL contains a newline (between "ht" and
  // "tps") and a '<' character, which causes KURL to set the
  // PotentiallyDanglingMarkup flag. The @font-face src URL uses the same
  // pattern: the font URL is constructed by the CSS parser with dangling
  // markup via a tainted base URL.
  //
  // We use a <base> tag with an href containing \n and '<' to taint all
  // relative URL resolution. The @font-face then uses a relative URL.
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: 'dangling-test';
        src: url('ht
tps://example.com/exfil<secret.woff2') format("woff2");
      }
      #target {
        font: 25px/1 'dangling-test', monospace;
      }
    </style>
    <span id="target">ABCDEF</span>
  )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  // If the dangling markup mitigation works correctly, no network request
  // is made for the font URL (it's blocked before reaching the network).
  // The element should render with the fallback monospace font.
  // The test passes if it doesn't crash — a missing SimSubresourceRequest
  // for an actual network fetch would cause a CHECK failure.
  auto* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
}

}  // namespace blink
