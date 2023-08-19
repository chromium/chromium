// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class LinkElementLoadingTest : public SimTest {};

TEST_F(LinkElementLoadingTest,
       ShouldCancelLoadingStyleSheetIfLinkElementIsDisconnected) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Write(
      "<!DOCTYPE html><link id=link rel=stylesheet href=test.css>");

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();

  // Remove a link element from a document
  auto* link =
      To<HTMLLinkElement>(GetDocument().getElementById(AtomicString("link")));
  EXPECT_NE(nullptr, link);
  link->remove();

  // Finish the load.
  css_resource.Complete();
  main_resource.Finish();

  // Link element's sheet loading should be canceled.
  EXPECT_EQ(nullptr, link->sheet());
}

}  // namespace blink
