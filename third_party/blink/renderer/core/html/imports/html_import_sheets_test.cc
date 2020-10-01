// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class HTMLImportSheetsTest : public SimTest {
 protected:
  HTMLImportSheetsTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  }
};

TEST_F(HTMLImportSheetsTest, NeedsActiveStyleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest import_resource("https://example.com/import.html",
                                        "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<style>div{}</style>");

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  Document* import_doc =
      To<HTMLLinkElement>(GetDocument().getElementById("link"))->import();
  ASSERT_TRUE(import_doc);
  EXPECT_TRUE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());

  GetDocument().GetStyleEngine().UpdateActiveStyle();

  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  EXPECT_FALSE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(HTMLImportSheetsTest, UpdateStyleSheetList) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest import_resource("https://example.com/import.html",
                                        "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<style>div{}</style>");

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  Document* import_doc =
      To<HTMLLinkElement>(GetDocument().getElementById("link"))->import();
  ASSERT_TRUE(import_doc);
  EXPECT_TRUE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());

  import_doc->GetStyleEngine().StyleSheetsForStyleSheetList(*import_doc);

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  EXPECT_TRUE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());
}

}  // namespace blink
