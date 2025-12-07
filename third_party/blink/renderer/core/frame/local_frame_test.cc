// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#if BUILDFLAG(IS_MAC)
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/input/text_input_host.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#endif

namespace blink {

namespace {

#if BUILDFLAG(IS_MAC)
void RegisterMockedHttpURLLoad(const std::string& base_url,
                               const std::string& file_name) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8(file_name));
}
#endif

}  // namespace

class LocalFrameTest : public testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

namespace {

void TestGreenDiv(DummyPageHolder& page_holder) {
  const Document& doc = page_holder.GetDocument();
  Element* div = doc.getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  ASSERT_TRUE(div->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      div->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

}  // namespace

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLStyleInBody) {
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));

  SegmentedBuffer data;
  data.Append(base::span_from_cstring(
      "<html xmlns='http://www.w3.org/1999/xhtml'><body><style>div { color: "
      "green }</style><div id='div'></div></body></html>"));
  page_holder->GetFrame().ForceSynchronousDocumentInstall(
      AtomicString("text/xml"), data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLLinkInBody) {
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));

  SegmentedBuffer data;
  data.Append(base::span_from_cstring(
      "<html xmlns='http://www.w3.org/1999/xhtml'><body><link rel='stylesheet' "
      "href='data:text/css,div{color:green}' /><div "
      "id='div'></div></body></html>"));
  page_holder->GetFrame().ForceSynchronousDocumentInstall(
      AtomicString("text/xml"), data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLStyleInHead) {
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));

  SegmentedBuffer data;
  data.Append(base::span_from_cstring(
      "<html xmlns='http://www.w3.org/1999/xhtml'><head><style>div { color: "
      "green }</style></head><body><div id='div'></div></body></html>"));
  page_holder->GetFrame().ForceSynchronousDocumentInstall(
      AtomicString("text/xml"), data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLLinkInHead) {
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));

  SegmentedBuffer data;
  data.Append(base::span_from_cstring(
      "<html xmlns='http://www.w3.org/1999/xhtml'><head><link rel='stylesheet' "
      "href='data:text/css,div{color:green}' /></head><body><div "
      "id='div'></div></body></html>"));
  page_holder->GetFrame().ForceSynchronousDocumentInstall(
      AtomicString("text/xml"), data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XMLStyleSheet) {
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));

  SegmentedBuffer data;
  data.Append(
      base::span_from_cstring("<?xml-stylesheet type='text/css' "
                              "href='data:text/css,div{color:green}'?><html "
                              "xmlns='http://www.w3.org/1999/xhtml'><body><div "
                              "id='div'></div></body></html>"));
  page_holder->GetFrame().ForceSynchronousDocumentInstall(
      AtomicString("text/xml"), data);
  TestGreenDiv(*page_holder);
}

#if BUILDFLAG(IS_MAC)
TEST_F(LocalFrameTest, CharacterIndexAtPointWithPinchZoom) {
  RegisterMockedHttpURLLoad("http://internal.test/", "sometext.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("http://internal.test/sometext.html");
  web_view_helper.LoadAhem();
  web_view_helper.Resize(gfx::Size(640, 480));

  // Move the visual viewport to the start of the target div containing the
  // text.
  web_view_helper.GetWebView()->SetPageScaleFactor(2);
  web_view_helper.GetWebView()->SetVisualViewportOffset(gfx::PointF(100, 50));

  Page* page = web_view_helper.GetWebView()->GetPage();
  LocalFrame* main_frame = DynamicTo<LocalFrame>(page->MainFrame());

  // Since we're zoomed in to 2X, each char of Ahem is 20px wide/tall in
  // viewport space. We expect to hit the fifth char on the first line.
  uint32_t index = main_frame->GetCharacterIndexAtPoint(gfx::Point(100, 15));
  EXPECT_EQ(index, 5ul);
}
#endif
}  // namespace blink
