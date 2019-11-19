// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_frame_serializer_test_helper.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class WebFrameSerializerImportTest : public SimTest {
 protected:
  WebFrameSerializerImportTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(WebSize(500, 500));
  }
};

TEST_F(WebFrameSerializerImportTest,
       AddStylesheetFromStyleElementInHtmlImport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest import_resource("https://example.com/import.html",
                                        "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<style>div { color: blue; }</style>");

  Document* import_doc =
      ToHTMLLinkElement(GetDocument().getElementById("link"))->import();
  ASSERT_TRUE(import_doc);
  import_doc->GetStyleEngine().StyleSheetsForStyleSheetList(*import_doc);

  String mhtml = WebFrameSerializerTestHelper::GenerateMHTML(&MainFrame());

  // The CSS styles defined in imported HTML should be added.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("div { color: blue; }"));
}

TEST_F(WebFrameSerializerImportTest, AddStylesheetFromLinkElementInHtmlImport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest import_resource("https://example.com/import.html",
                                        "text/html");
  SimSubresourceRequest import_css("https://example.com/import.css",
                                   "text/css");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<link rel=import href=import.css>");
  import_css.Complete("<style>div { color: red; }</style>");

  Document* import_doc =
      ToHTMLLinkElement(GetDocument().getElementById("link"))->import();
  ASSERT_TRUE(import_doc);
  import_doc->GetStyleEngine().StyleSheetsForStyleSheetList(*import_doc);

  String mhtml = WebFrameSerializerTestHelper::GenerateMHTML(&MainFrame());

  // The CSS styles defined in imported HTML should be added.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("div { color: red; }"));
}

}  // namespace blink
