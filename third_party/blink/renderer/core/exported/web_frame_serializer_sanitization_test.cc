/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_frame_serializer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/exported/web_frame_serializer_test_helper.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

// Returns the count of match for substring |pattern| in string |str|.
int MatchSubstring(const String& str, const char* pattern, size_t size) {
  int matches = 0;
  size_t start = 0;
  while (true) {
    size_t pos = str.Find(pattern, start);
    if (pos == WTF::kNotFound)
      break;
    matches++;
    start = pos + size;
  }
  return matches;
}

}  // namespace

class WebFrameSerializerSanitizationTest : public testing::Test {
 protected:
  WebFrameSerializerSanitizationTest() { helper_.Initialize(); }

  ~WebFrameSerializerSanitizationTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  String GenerateMHTMLFromHtml(const String& url, const String& file_name) {
    LoadFrame(url, file_name, "text/html");
    return WebFrameSerializerTestHelper::GenerateMHTML(MainFrameImpl());
  }

  String GenerateMHTMLPartsFromPng(const String& url, const String& file_name) {
    LoadFrame(url, file_name, "image/png");
    return WebFrameSerializerTestHelper::GenerateMHTMLParts(MainFrameImpl());
  }

  void LoadFrame(const String& url,
                 const String& file_name,
                 const String& mime_type) {
    KURL parsed_url(url);
    String file_path("frameserialization/" + file_name);
    RegisterMockedFileURLLoad(parsed_url, file_path, mime_type);
    frame_test_helpers::LoadFrame(MainFrameImpl(), url.Utf8().c_str());
    MainFrameImpl()->GetFrame()->View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
    MainFrameImpl()->GetFrame()->GetDocument()->UpdateStyleAndLayoutTree();
    test::RunPendingTasks();
  }

  ShadowRoot* SetShadowContent(TreeScope& scope,
                               const char* host,
                               ShadowRootType shadow_type,
                               const char* shadow_content,
                               bool delegates_focus = false) {
    Element* host_element = scope.getElementById(AtomicString::FromUTF8(host));
    ShadowRoot* shadow_root;
    if (shadow_type == ShadowRootType::V0) {
      DCHECK(!delegates_focus);
      shadow_root = &host_element->CreateV0ShadowRootForTesting();
    } else {
      shadow_root =
          &host_element->AttachShadowRootInternal(shadow_type, delegates_focus);
    }
    shadow_root->SetDelegatesFocus(delegates_focus);
    shadow_root->SetInnerHTMLFromString(String::FromUTF8(shadow_content),
                                        ASSERT_NO_EXCEPTION);
    scope.GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
    return shadow_root;
  }

  void RegisterMockedFileURLLoad(const KURL& url,
                                 const String& file_path,
                                 const String& mime_type = "image/png") {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoad(
        url, test::CoreTestDataPath(file_path.Utf8().c_str()), mime_type);
  }

  WebViewImpl* WebView() { return helper_.GetWebView(); }

  WebLocalFrameImpl* MainFrameImpl() { return helper_.LocalMainFrame(); }

 private:
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(WebFrameSerializerSanitizationTest, RemoveInlineScriptInAttributes) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "script_in_attributes.html");

  // These scripting attributes should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("onload="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("ONLOAD="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("onclick="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("href="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("from="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("to="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("javascript:"));

  // These non-scripting attributes should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("class="));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id="));

  // srcdoc attribute of frame element should be replaced with src attribute.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("srcdoc="));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("src="));
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveOtherAttributes) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "remove_attributes.html");
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("ping="));
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveHiddenElements) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "hidden_elements.html");

  // The element with hidden attribute should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<p id=3D\"hidden_id\""));

  // The hidden form element should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<input type=3D\"hidden\""));

  // The style element should be converted to link element.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<style"));

  // All other hidden elements should not be removed.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<html"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<head"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<title"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<h1"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<h2"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<datalist"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<option"));
  // One for meta in head and another for meta in body.
  EXPECT_EQ(2, MatchSubstring(mhtml, "<meta", 5));
  // Two for original link elements: one in head and another in body.
  // Two for original style elemtns: one in head and another in body.
  EXPECT_EQ(4, MatchSubstring(mhtml, "<link", 5));

  // These visible elements should remain intact.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<p id=3D\"visible_id\""));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<form"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<input type=3D\"text\""));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<div"));
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveIframeInHead) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "iframe_in_head.html");

  // The iframe elements could only be found after body. Any iframes injected to
  // head should be removed.
  EXPECT_GT(mhtml.Find("<iframe"), mhtml.Find("<body"));
}

// Regression test for crbug.com/678893, where in some cases serializing an
// image document could cause code to pick an element from an empty container.
TEST_F(WebFrameSerializerSanitizationTest, FromBrokenImageDocument) {
  // This test only cares that the result of the parts generation is empty so it
  // is simpler to not generate only that instead of the full MHTML.
  String mhtml =
      GenerateMHTMLPartsFromPng("http://www.test.com", "broken-image.png");
  EXPECT_TRUE(mhtml.IsEmpty());
}

TEST_F(WebFrameSerializerSanitizationTest, ImageLoadedFromSrcsetForHiDPI) {
  RegisterMockedFileURLLoad(KURL("http://www.test.com/1x.png"),
                            "frameserialization/1x.png");
  RegisterMockedFileURLLoad(KURL("http://www.test.com/2x.png"),
                            "frameserialization/2x.png");

  // Set high DPR in order to load image from srcset, instead of src.
  WebView()->SetDeviceScaleFactor(2.0f);

  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "img_srcset.html");

  // srcset and sizes attributes should be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("srcset="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("sizes="));

  // src attribute with original URL should be preserved.
  EXPECT_EQ(2,
            MatchSubstring(mhtml, "src=3D\"http://www.test.com/1x.png\"", 34));

  // The image resource for original URL should be attached.
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("Content-Location: http://www.test.com/1x.png"));

  // Width and height attributes should be set when none is present in <img>.
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("id=3D\"i1\" width=3D\"6\" height=3D\"6\">"));

  // Height attribute should not be set if width attribute is already present in
  // <img>
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id=3D\"i2\" width=3D\"8\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, ImageLoadedFromSrcForNormalDPI) {
  RegisterMockedFileURLLoad(KURL("http://www.test.com/1x.png"),
                            "frameserialization/1x.png");
  RegisterMockedFileURLLoad(KURL("http://www.test.com/2x.png"),
                            "frameserialization/2x.png");

  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "img_srcset.html");

  // srcset and sizes attributes should be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("srcset="));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("sizes="));

  // src attribute with original URL should be preserved.
  EXPECT_EQ(2,
            MatchSubstring(mhtml, "src=3D\"http://www.test.com/1x.png\"", 34));

  // The image resource for original URL should be attached.
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("Content-Location: http://www.test.com/1x.png"));

  // New width and height attributes should not be set.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id=3D\"i1\">"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("id=3D\"i2\" width=3D\"8\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, RemovePopupOverlayIfRequested) {
  WebView()->MainFrameWidget()->Resize(WebSize(500, 500));
  LoadFrame("http://www.test.com", "popup.html", "text/html");
  String mhtml =
      WebFrameSerializerTestHelper::GenerateMHTMLWithPopupOverlayRemoved(
          MainFrameImpl());
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("class=3D\"overlay"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("class=3D\"modal"));
}

TEST_F(WebFrameSerializerSanitizationTest, PopupOverlayNotFound) {
  WebView()->MainFrameWidget()->Resize(WebSize(500, 500));
  LoadFrame("http://www.test.com", "text_only_page.html", "text/html");
  WebFrameSerializerTestHelper::GenerateMHTMLWithPopupOverlayRemoved(
      MainFrameImpl());
}

TEST_F(WebFrameSerializerSanitizationTest, KeepPopupOverlayIfNotRequested) {
  WebView()->MainFrameWidget()->Resize(WebSize(500, 500));
  String mhtml = GenerateMHTMLFromHtml("http://www.test.com", "popup.html");
  EXPECT_NE(WTF::kNotFound, mhtml.Find("class=3D\"overlay"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("class=3D\"modal"));
}

TEST_F(WebFrameSerializerSanitizationTest, LinkIntegrity) {
  RegisterMockedFileURLLoad(KURL("http://www.test.com/beautifull.css"),
                            "frameserialization/beautifull.css", "text/css");
  RegisterMockedFileURLLoad(KURL("http://www.test.com/integrityfail.css"),
                            "frameserialization/integrityfail.css", "text/css");
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "link_integrity.html");
  SCOPED_TRACE(testing::Message() << "mhtml:\n" << mhtml);

  // beautifull.css remains, without 'integrity'. integrityfail.css is removed.
  EXPECT_TRUE(
      mhtml.Contains("<link rel=3D\"stylesheet\" "
                     "href=3D\"http://www.test.com/beautifull.css\">"));
  EXPECT_EQ(WTF::kNotFound,
            mhtml.Find("http://www.test.com/integrityfail.css"));
}

TEST_F(WebFrameSerializerSanitizationTest, RemoveElements) {
  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "remove_elements.html");

  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<script"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<noscript"));

  // Only the meta element containing "Content-Security-Policy" is removed.
  // Other meta elements should be preserved.
  EXPECT_EQ(WTF::kNotFound,
            mhtml.Find("<meta http-equiv=3D\"Content-Security-Policy"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<meta name=3D\"description"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<meta http-equiv=3D\"refresh"));

  // If an element is removed, its children should also be skipped.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<select"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("<option"));
}

TEST_F(WebFrameSerializerSanitizationTest, ShadowDOM) {
  LoadFrame("http://www.test.com", "shadow_dom.html", "text/html");
  Document* document = MainFrameImpl()->GetFrame()->GetDocument();
  SetShadowContent(*document, "h1", ShadowRootType::V0, "V0 shadow");
  ShadowRoot* shadowRoot =
      SetShadowContent(*document, "h2", ShadowRootType::kOpen,
                       "Parent shadow\n<p id=\"h3\">Foo</p>", true);
  SetShadowContent(*shadowRoot, "h3", ShadowRootType::kClosed, "Nested shadow");
  String mhtml = WebFrameSerializerTestHelper::GenerateMHTML(MainFrameImpl());

  // Template with special attribute should be created for each shadow DOM tree.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<template shadowmode=3D\"v0\">"));
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("<template shadowmode=3D\"open\" shadowdelegatesfocus"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("<template shadowmode=3D\"closed\">"));

  // The special attribute present in the original page should be removed.
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("shadowmode=3D\"foo\">"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("shadowdelegatesfocus=3D\"bar\">"));
}

TEST_F(WebFrameSerializerSanitizationTest, StyleElementsWithDynamicCSS) {
  String mhtml = GenerateMHTMLFromHtml("http://www.test.com",
                                       "style_element_with_dynamic_css.html");

  // The dynamically updated CSS rules should be preserved.
  EXPECT_NE(WTF::kNotFound, mhtml.Find("div { color: blue; }"));
  EXPECT_NE(WTF::kNotFound, mhtml.Find("p { color: red; }"));
  EXPECT_EQ(WTF::kNotFound, mhtml.Find("h1 { color: green; }"));
}

TEST_F(WebFrameSerializerSanitizationTest, PictureElement) {
  RegisterMockedFileURLLoad(KURL("http://www.test.com/1x.png"),
                            "frameserialization/1x.png");
  RegisterMockedFileURLLoad(KURL("http://www.test.com/2x.png"),
                            "frameserialization/2x.png");

  WebView()->MainFrameWidget()->Resize(WebSize(500, 500));

  String mhtml = GenerateMHTMLFromHtml("http://www.test.com", "picture.html");

  // srcset attribute should be kept.
  EXPECT_EQ(2, MatchSubstring(mhtml, "srcset=", 7));

  // 2x.png resource should be added.
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("Content-Location: http://www.test.com/2x.png"));
  EXPECT_EQ(WTF::kNotFound,
            mhtml.Find("Content-Location: http://www.test.com/1x.png"));
}

TEST_F(WebFrameSerializerSanitizationTest, ImageInPluginElement) {
  RegisterMockedFileURLLoad(KURL("http://www.test.com/1x.png"),
                            "frameserialization/1x.png");
  RegisterMockedFileURLLoad(KURL("http://www.test.com/2x.png"),
                            "frameserialization/2x.png");

  String mhtml =
      GenerateMHTMLFromHtml("http://www.test.com", "image_in_plugin.html");

  // Image resources for both object and embed elements should be added.
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("Content-Location: http://www.test.com/1x.png"));
  EXPECT_NE(WTF::kNotFound,
            mhtml.Find("Content-Location: http://www.test.com/2x.png"));
}

}  // namespace blink
