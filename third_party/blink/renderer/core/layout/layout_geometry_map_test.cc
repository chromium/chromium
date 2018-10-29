/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class LayoutGeometryMapTest : public testing::Test {
 public:
  LayoutGeometryMapTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  static LayoutBox* GetFrameElement(const char* iframe_name,
                                    WebView* web_view,
                                    const WTF::AtomicString& element_id) {
    WebFrame* iframe =
        static_cast<WebViewImpl*>(web_view)->MainFrameImpl()->FindFrameByName(
            WebString::FromUTF8(iframe_name));
    if (!iframe || !iframe->IsWebLocalFrame())
      return nullptr;
    LocalFrame* frame = ToWebLocalFrameImpl(iframe)->GetFrame();
    Document* doc = frame->GetDocument();
    Element* element = doc->getElementById(element_id);
    if (!element)
      return nullptr;
    return element->GetLayoutBox();
  }

  static Element* GetElement(WebView* web_view,
                             const WTF::AtomicString& element_id) {
    WebViewImpl* web_view_impl = static_cast<WebViewImpl*>(web_view);
    if (!web_view_impl)
      return nullptr;
    LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
    Document* doc = frame->GetDocument();
    return doc->getElementById(element_id);
  }

  static LayoutBox* GetLayoutBox(WebView* web_view,
                                 const WTF::AtomicString& element_id) {
    Element* element = GetElement(web_view, element_id);
    if (!element)
      return nullptr;
    return element->GetLayoutBox();
  }

  static const LayoutBoxModelObject* GetLayoutContainer(
      WebView* web_view,
      const WTF::AtomicString& element_id) {
    LayoutBox* rb = GetLayoutBox(web_view, element_id);
    if (!rb)
      return nullptr;
    PaintLayer* compositing_layer =
        rb->EnclosingLayer()->EnclosingLayerForPaintInvalidation();
    if (!compositing_layer)
      return nullptr;
    return &compositing_layer->GetLayoutObject();
  }

  static const LayoutBoxModelObject* GetFrameLayoutContainer(
      const char* frame_id,
      WebView* web_view,
      const WTF::AtomicString& element_id) {
    LayoutBox* rb = GetFrameElement(frame_id, web_view, element_id);
    if (!rb)
      return nullptr;
    PaintLayer* compositing_layer =
        rb->EnclosingLayer()->EnclosingLayerForPaintInvalidation();
    if (!compositing_layer)
      return nullptr;
    return &compositing_layer->GetLayoutObject();
  }

  static const FloatRect RectFromQuad(const FloatQuad& quad) {
    FloatRect rect;
    rect.SetX(std::min(
        quad.P1().X(),
        std::min(quad.P2().X(), std::min(quad.P3().X(), quad.P4().X()))));
    rect.SetY(std::min(
        quad.P1().Y(),
        std::min(quad.P2().Y(), std::min(quad.P3().Y(), quad.P4().Y()))));

    rect.SetWidth(std::max(quad.P1().X(),
                           std::max(quad.P2().X(),
                                    std::max(quad.P3().X(), quad.P4().X()))) -
                  rect.X());
    rect.SetHeight(std::max(quad.P1().Y(),
                            std::max(quad.P2().Y(),
                                     std::max(quad.P3().Y(), quad.P4().Y()))) -
                   rect.Y());
    return rect;
  }

  // Adjust rect by the scroll offset of the LayoutView.  This only has an
  // effect if root layer scrolling is enabled.  The only reason for doing
  // this here is so the test expected values can be the same whether or not
  // root layer scrolling is enabled.  For more context, see:
  // https://codereview.chromium.org/2417103002/#msg11
  static FloatRect AdjustForFrameScroll(WebView* web_view,
                                        const FloatRect& rect) {
    FloatRect result(rect);
    LocalFrame* frame =
        static_cast<WebViewImpl*>(web_view)->MainFrameImpl()->GetFrame();
    LayoutView* layout_view = frame->GetDocument()->GetLayoutView();
    if (layout_view->HasOverflowClip())
      result.Move(layout_view->ScrolledContentOffset());
    return result;
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  const std::string base_url_;
};

TEST_F(LayoutGeometryMapTest, SimpleGeometryMapTest) {
  RegisterMockedHttpURLLoad("rgm_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "rgm_test.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  // We are going test everything twice. Once with FloatPoints and once with
  // FloatRects. This is because LayoutGeometryMap treats both slightly
  // differently
  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InitialDiv"), nullptr);
  FloatRect rect(0.0f, 0.0f, 1.0f, 2.0f);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(21.0f, 6.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "OtherDiv"),
                             GetLayoutBox(web_view, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(22.0f, 12.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(1.0f, 6.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "InnerDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(50.0f, 44.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, nullptr));
}

// Fails on Windows due to crbug.com/391457. When run through the transform the
// position on windows differs by a pixel
#if defined(OS_WIN)
TEST_F(LayoutGeometryMapTest, DISABLED_TransformedGeometryTest)
#else
TEST_F(LayoutGeometryMapTest, TransformedGeometryTest)
#endif
{
  RegisterMockedHttpURLLoad("rgm_transformed_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_transformed_test.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InitialDiv"), nullptr);
  const float kRectWidth = 15.0f;
  const float kScaleWidth = 2.0f;
  const float kScaleHeight = 3.0f;
  FloatRect rect(0.0f, 0.0f, 15.0f, 25.0f);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 15.0f, 25.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 15.0f, 25.0f)).BoundingBox(),
            rgm.MapToAncestor(rect, nullptr).BoundingBox());

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(523.0f - kRectWidth, 6.0f, 15.0f, 25.0f))
                .BoundingBox(),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv"))
                .BoundingBox());

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "OtherDiv"),
                             GetLayoutBox(web_view, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(522.0f - kRectWidth, 12.0f, 15.0f, 25.0f))
                .BoundingBox(),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv"))
                .BoundingBox());

  EXPECT_EQ(FloatQuad(FloatRect(1.0f, 6.0f, 15.0f, 25.0f)).BoundingBox(),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "InnerDiv"))
                .BoundingBox());

  EXPECT_EQ(FloatQuad(FloatRect(821.0f - kRectWidth * kScaleWidth, 31.0f,
                                15.0f * kScaleWidth, 25.0f * kScaleHeight))
                .BoundingBox(),
            rgm.MapToAncestor(rect, nullptr).BoundingBox());

  rect = FloatRect(10.0f, 25.0f, 15.0f, 25.0f);
  EXPECT_EQ(FloatQuad(FloatRect(512.0f - kRectWidth, 37.0f, 15.0f, 25.0f))
                .BoundingBox(),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv"))
                .BoundingBox());

  EXPECT_EQ(FloatQuad(FloatRect(11.0f, 31.0f, 15.0f, 25.0f)).BoundingBox(),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "InnerDiv"))
                .BoundingBox());

  EXPECT_EQ(FloatQuad(FloatRect(801.0f - kRectWidth * kScaleWidth, 106.0f,
                                15.0f * kScaleWidth, 25.0f * kScaleHeight))
                .BoundingBox(),
            rgm.MapToAncestor(rect, nullptr).BoundingBox());
}

TEST_F(LayoutGeometryMapTest, FixedGeometryTest) {
  RegisterMockedHttpURLLoad("rgm_fixed_position_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_fixed_position_test.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InitialDiv"), nullptr);
  FloatRect rect(0.0f, 0.0f, 15.0f, 25.0f);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 15.0f, 25.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 15.0f, 25.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(20.0f, 14.0f, 15.0f, 25.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "OtherDiv"),
                             GetLayoutBox(web_view, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(21.0f, 20.0f, 15.0f, 25.0f)),
            rgm.MapToAncestor(rect, GetLayoutContainer(web_view, "CenterDiv")));

  rect = FloatRect(22.0f, 15.2f, 15.3f, 0.0f);
  EXPECT_EQ(FloatQuad(FloatRect(43.0f, 35.2f, 15.3f, 0.0f)),
            rgm.MapToAncestor(rect, GetLayoutContainer(web_view, "CenterDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(43.0f, 35.2f, 15.3f, 0.0f)),
            rgm.MapToAncestor(rect, GetLayoutContainer(web_view, "InnerDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(43.0f, 35.2f, 15.3f, 0.0f)),
            rgm.MapToAncestor(rect, nullptr));
}

TEST_F(LayoutGeometryMapTest, ContainsFixedPositionTest) {
  RegisterMockedHttpURLLoad("rgm_contains_fixed_position_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_contains_fixed_position_test.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  FloatRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  LayoutGeometryMap rgm;

  // This fixed position element is not contained and so is attached at the top
  // of the viewport.
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "simple-container"),
                             nullptr);
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "fixed1"),
                             GetLayoutBox(web_view, "simple-container"));
  EXPECT_EQ(FloatRect(8.0f, 50.0f, 100.0f, 100.0f),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  EXPECT_EQ(
      FloatQuad(FloatRect(0.0f, -50.0f, 100.0f, 100.0f)),
      rgm.MapToAncestor(rect, GetLayoutBox(web_view, "simple-container")));
  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  // Transforms contain fixed position descendants.
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "has-transform"), nullptr);
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "fixed2"),
                             GetLayoutBox(web_view, "has-transform"));
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  // Paint containment contains fixed position descendants.
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "contains-paint"), nullptr);
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "fixed3"),
                             GetLayoutBox(web_view, "contains-paint"));
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
}

TEST_F(LayoutGeometryMapTest, IframeTest) {
  RegisterMockedHttpURLLoad("rgm_iframe_test.html");
  RegisterMockedHttpURLLoad("rgm_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "rgm_iframe_test.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  LayoutGeometryMap rgm(kTraverseDocumentBoundaries);
  LayoutGeometryMap rgm_no_frame;

  rgm_no_frame.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InitialDiv"), nullptr);
  rgm.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InitialDiv"), nullptr);
  FloatRect rect(0.0f, 0.0f, 1.0f, 2.0f);

  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 1.0f, 2.0f)),
            rgm_no_frame.MapToAncestor(rect, nullptr));

  // Our initial rect looks like: (0, 0, 1, 2)
  //        p0_____
  //          |   |
  //          |   |
  //          |   |
  //          |___|
  // When we rotate we get a rect of the form:
  //         p0_
  //          / -_
  //         /   /
  //        /   /
  //         -_/
  // So it is sensible that the minimum y is the same as for a point at 0, 0.
  // The minimum x should be p0.x - 2 * sin(30deg) = p0.x - 1.
  // That maximum x should likewise be p0.x + cos(30deg) = p0.x + 0.866.
  // And the maximum y should be p0.x + sin(30deg) + 2*cos(30deg)
  //      = p0.y + 2.232.
  EXPECT_NEAR(69.5244f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).X(),
              0.0001f);
  EXPECT_NEAR(-44.0237, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Y(),
              0.0001f);
  EXPECT_NEAR(1.866, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Width(),
              0.0001f);
  EXPECT_NEAR(2.232, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Height(),
              0.0001f);

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm_no_frame.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  rgm.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InnerDiv"), nullptr);
  rgm_no_frame.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(21.0f, 6.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, GetFrameLayoutContainer(
                                        "test_frame", web_view, "CenterDiv")));
  EXPECT_EQ(
      FloatQuad(FloatRect(21.0f, 6.0f, 1.0f, 2.0f)),
      rgm_no_frame.MapToAncestor(
          rect, GetFrameLayoutContainer("test_frame", web_view, "CenterDiv")));

  rgm.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "OtherDiv"),
      GetFrameLayoutContainer("test_frame", web_view, "InnerDiv"));
  rgm_no_frame.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "OtherDiv"),
      GetFrameLayoutContainer("test_frame", web_view, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(22.0f, 12.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, GetFrameLayoutContainer(
                                        "test_frame", web_view, "CenterDiv")));
  EXPECT_EQ(
      FloatQuad(FloatRect(22.0f, 12.0f, 1.0f, 2.0f)),
      rgm_no_frame.MapToAncestor(
          rect, GetFrameLayoutContainer("test_frame", web_view, "CenterDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(1.0f, 6.0f, 1.0f, 2.0f)),
            rgm.MapToAncestor(rect, GetFrameLayoutContainer(
                                        "test_frame", web_view, "InnerDiv")));
  EXPECT_EQ(
      FloatQuad(FloatRect(1.0f, 6.0f, 1.0f, 2.0f)),
      rgm_no_frame.MapToAncestor(
          rect, GetFrameLayoutContainer("test_frame", web_view, "InnerDiv")));

  EXPECT_NEAR(87.8975f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).X(),
              0.0001f);
  EXPECT_NEAR(8.1532f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Y(),
              0.0001f);
  EXPECT_NEAR(1.866, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Width(),
              0.0001f);
  EXPECT_NEAR(2.232, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Height(),
              0.0001f);

  EXPECT_EQ(FloatQuad(FloatRect(50.0f, 44.0f, 1.0f, 2.0f)),
            rgm_no_frame.MapToAncestor(rect, nullptr));
}

TEST_F(LayoutGeometryMapTest, ColumnTest) {
  RegisterMockedHttpURLLoad("rgm_column_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "rgm_column_test.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  // The document is 1000f wide (we resized to that size).
  // We have a 8px margin on either side of the document.
  // Between each column we have a 10px gap, and we have 3 columns.
  // The width of a given column is (1000 - 16 - 20)/3.
  // The total offset between any column and it's neighbour is width + 10px
  // (for the gap).
  float offset = (1000.0f - 16.0f - 20.0f) / 3.0f + 10.0f;

  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "A"), nullptr);
  FloatRect rect(0.0f, 0.0f, 5.0f, 3.0f);

  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 5.0f, 3.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 5.0f, 3.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "Col1"), nullptr);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 5.0f, 3.0f)),
            rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "Col2"), nullptr);
  EXPECT_NEAR(8.0f + offset, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).X(),
              0.1f);
  EXPECT_NEAR(8.0f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Y(), 0.1f);
  EXPECT_EQ(5.0f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Width());
  EXPECT_EQ(3.0f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Height());

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "Col3"), nullptr);
  EXPECT_NEAR(8.0f + offset * 2.0f,
              RectFromQuad(rgm.MapToAncestor(rect, nullptr)).X(), 0.1f);
  EXPECT_NEAR(8.0f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Y(), 0.1f);
  EXPECT_EQ(5.0f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Width());
  EXPECT_EQ(3.0f, RectFromQuad(rgm.MapToAncestor(rect, nullptr)).Height());
}

TEST_F(LayoutGeometryMapTest, FloatUnderInlineLayer) {
  RegisterMockedHttpURLLoad("rgm_float_under_inline.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_float_under_inline.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  LayoutGeometryMap rgm;
  auto* layer_under_float = GetLayoutBox(web_view, "layer-under-float");
  auto* span = GetElement(web_view, "span")->GetLayoutBoxModelObject();
  auto* floating = GetLayoutBox(web_view, "float");
  auto* container = GetLayoutBox(web_view, "container");
  FloatRect rect(3.0f, 4.0f, 10.0f, 8.0f);

  rgm.PushMappingsToAncestor(container->Layer(), nullptr);
  rgm.PushMappingsToAncestor(span->Layer(), container->Layer());
  rgm.PushMappingsToAncestor(layer_under_float->Layer(), span->Layer());
  EXPECT_EQ(rect, RectFromQuad(rgm.MapToAncestor(rect, container)));
  EXPECT_EQ(FloatRect(63.0f, 54.0f, 10.0f, 8.0f),
            RectFromQuad(rgm.MapToAncestor(rect, nullptr)));

  rgm.PopMappingsToAncestor(span->Layer());
  EXPECT_EQ(FloatRect(203.0f, 104.0f, 10.0f, 8.0f),
            RectFromQuad(rgm.MapToAncestor(rect, container)));
  EXPECT_EQ(FloatRect(263.0f, 154.0f, 10.0f, 8.0f),
            RectFromQuad(rgm.MapToAncestor(rect, nullptr)));

  rgm.PushMappingsToAncestor(floating, span);
  EXPECT_EQ(rect, RectFromQuad(rgm.MapToAncestor(rect, container)));
  EXPECT_EQ(FloatRect(63.0f, 54.0f, 10.0f, 8.0f),
            RectFromQuad(rgm.MapToAncestor(rect, nullptr)));
}

}  // namespace blink
