// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class TextSelectionRepaintTest : public SimTest {};

TEST_F(TextSelectionRepaintTest, RepaintSelectionOnFocus) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(
      "<!DOCTYPE html>"
      "Text to select.");

  // Focus the window.
  EXPECT_FALSE(GetPage().IsFocused());
  GetPage().SetFocused(true);

  // First frame with nothing selected.
  Compositor().BeginFrame();

  // Select some text.
  auto* body = GetDocument().body();
  Window().getSelection()->setBaseAndExtent(body, 0, body, 1);

  // Unfocus the page and check for a pending frame.
  GetPage().SetFocused(false);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  // Frame with the unfocused selection appearance.
  Compositor().BeginFrame();

  // Focus the page and check for a pending frame.
  GetPage().SetFocused(true);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

}  // namespace blink
