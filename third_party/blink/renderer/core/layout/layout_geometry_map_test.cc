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
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class LayoutGeometryMapTest : public testing::Test {
 public:
  LayoutGeometryMapTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  static LayoutBox* GetFrameElement(const char* iframe_name,
                                    WebView* web_view,
                                    const WTF::AtomicString& element_id) {
    WebFrame* iframe =
        static_cast<WebViewImpl*>(web_view)->MainFrameImpl()->FindFrameByName(
            WebString::FromUTF8(iframe_name));
    auto* web_local_frame = DynamicTo<WebLocalFrameImpl>(iframe);
    if (!web_local_frame)
      return nullptr;
    LocalFrame* frame = web_local_frame->GetFrame();
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

  // Adjust rect by the scroll offset of the LayoutView.  This only has an
  // effect if root layer scrolling is enabled.  The only reason for doing
  // this here is so the test expected values can be the same whether or not
  // root layer scrolling is enabled.  For more context, see:
  // https://codereview.chromium.org/2417103002/#msg11
  static PhysicalRect AdjustForFrameScroll(WebView* web_view,
                                           const PhysicalRect& rect) {
    PhysicalRect result(rect);
    LocalFrame* frame =
        static_cast<WebViewImpl*>(web_view)->MainFrameImpl()->GetFrame();
    LayoutView* layout_view = frame->GetDocument()->GetLayoutView();
    if (layout_view->HasOverflowClip())
      result.Move(PhysicalOffset(layout_view->ScrolledContentOffset()));
    return result;
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void UpdateAllLifecyclePhases(WebView* web_view) {
    web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  const std::string base_url_;
};

TEST_F(LayoutGeometryMapTest, SimpleGeometryMapTest) {
  RegisterMockedHttpURLLoad("rgm_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "rgm_test.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  // We are going test everything twice. Once with FloatPoints and once with
  // FloatRects. This is because LayoutGeometryMap treats both slightly
  // differently
  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InitialDiv"), nullptr);
  PhysicalRect rect(0, 0, 1, 2);
  EXPECT_EQ(PhysicalRect(8, 8, 1, 2), rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(rect, rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(PhysicalRect(21, 6, 1, 2),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "OtherDiv"),
                             GetLayoutBox(web_view, "InnerDiv"));
  EXPECT_EQ(PhysicalRect(22, 12, 1, 2),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  EXPECT_EQ(PhysicalRect(1, 6, 1, 2),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "InnerDiv")));

  EXPECT_EQ(PhysicalRect(50, 44, 1, 2), rgm.MapToAncestor(rect, nullptr));
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
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InitialDiv"), nullptr);
  const int kRectWidth = 15;
  const int kScaleWidth = 2;
  const int kScaleHeight = 3;
  PhysicalRect rect(0, 0, 15, 25);
  EXPECT_EQ(PhysicalRect(8, 8, 15, 25), rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(PhysicalRect(0, 0, 15, 25), rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(PhysicalRect(523.0f - kRectWidth, 6, 15, 25),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "OtherDiv"),
                             GetLayoutBox(web_view, "InnerDiv"));
  EXPECT_EQ(PhysicalRect(522.0f - kRectWidth, 12, 15, 25),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  EXPECT_EQ(PhysicalRect(1, 6, 15, 25),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "InnerDiv")));

  EXPECT_EQ(PhysicalRect(821 - kRectWidth * kScaleWidth, 31, 15 * kScaleWidth,
                         25 * kScaleHeight),
            rgm.MapToAncestor(rect, nullptr));

  rect = PhysicalRect(10, 25, 15, 25);
  EXPECT_EQ(PhysicalRect(512.0f - kRectWidth, 37, 15, 25),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "CenterDiv")));

  EXPECT_EQ(PhysicalRect(11, 31, 15, 25),
            rgm.MapToAncestor(rect, GetLayoutBox(web_view, "InnerDiv")));

  EXPECT_EQ(PhysicalRect(801 - kRectWidth * kScaleWidth, 106, 15 * kScaleWidth,
                         25 * kScaleHeight),
            rgm.MapToAncestor(rect, nullptr));
}

TEST_F(LayoutGeometryMapTest, FixedGeometryTest) {
  RegisterMockedHttpURLLoad("rgm_fixed_position_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_fixed_position_test.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InitialDiv"), nullptr);
  PhysicalRect rect(0, 0, 15, 25);
  EXPECT_EQ(PhysicalRect(8, 8, 15, 25), rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(PhysicalRect(0, 0, 15, 25), rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(PhysicalRect(20, 14, 15, 25), rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "OtherDiv"),
                             GetLayoutBox(web_view, "InnerDiv"));
  EXPECT_EQ(PhysicalRect(21, 20, 15, 25),
            rgm.MapToAncestor(rect, GetLayoutContainer(web_view, "CenterDiv")));

  rect = PhysicalRect(LayoutUnit(22), LayoutUnit(15.2f), LayoutUnit(15.3f),
                      LayoutUnit());
  EXPECT_EQ(PhysicalRect(LayoutUnit(43), LayoutUnit(35.2f), rect.Width(),
                         rect.Height()),
            rgm.MapToAncestor(rect, GetLayoutContainer(web_view, "CenterDiv")));

  EXPECT_EQ(PhysicalRect(LayoutUnit(43), LayoutUnit(35.2f), rect.Width(),
                         rect.Height()),
            rgm.MapToAncestor(rect, GetLayoutContainer(web_view, "InnerDiv")));

  EXPECT_EQ(PhysicalRect(LayoutUnit(43), LayoutUnit(35.2f), rect.Width(),
                         rect.Height()),
            rgm.MapToAncestor(rect, nullptr));
}

TEST_F(LayoutGeometryMapTest, ContainsFixedPositionTest) {
  RegisterMockedHttpURLLoad("rgm_contains_fixed_position_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_contains_fixed_position_test.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  PhysicalRect rect(0, 0, 100, 100);
  LayoutGeometryMap rgm;

  // This fixed position element is not contained and so is attached at the top
  // of the viewport.
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "simple-container"),
                             nullptr);
  EXPECT_EQ(PhysicalRect(8, 100, 100, 100),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "fixed1"),
                             GetLayoutBox(web_view, "simple-container"));
  EXPECT_EQ(PhysicalRect(8, 50, 100, 100),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  EXPECT_EQ(
      PhysicalRect(0, -50, 100, 100),
      rgm.MapToAncestor(rect, GetLayoutBox(web_view, "simple-container")));
  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  // Transforms contain fixed position descendants.
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "has-transform"), nullptr);
  EXPECT_EQ(PhysicalRect(8, 100, 100, 100),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "fixed2"),
                             GetLayoutBox(web_view, "has-transform"));
  EXPECT_EQ(PhysicalRect(8, 100, 100, 100),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  // Paint containment contains fixed position descendants.
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "contains-paint"), nullptr);
  EXPECT_EQ(PhysicalRect(8, 100, 100, 100),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "fixed3"),
                             GetLayoutBox(web_view, "contains-paint"));
  EXPECT_EQ(PhysicalRect(8, 100, 100, 100),
            AdjustForFrameScroll(web_view, rgm.AbsoluteRect(rect)));
  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
}

TEST_F(LayoutGeometryMapTest, IframeTest) {
  RegisterMockedHttpURLLoad("rgm_iframe_test.html");
  RegisterMockedHttpURLLoad("rgm_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "rgm_iframe_test.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  LayoutGeometryMap rgm(kTraverseDocumentBoundaries);
  LayoutGeometryMap rgm_no_frame;

  rgm_no_frame.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InitialDiv"), nullptr);
  rgm.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InitialDiv"), nullptr);
  PhysicalRect rect(0, 0, 1, 2);

  EXPECT_EQ(PhysicalRect(8, 8, 1, 2),
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
  FloatRect mapped_bounding_box =
      rgm.MapToAncestorQuad(rect, nullptr).BoundingBox();
  EXPECT_NEAR(69.5244f, mapped_bounding_box.X(), 0.0001f);
  EXPECT_NEAR(-44.0237, mapped_bounding_box.Y(), 0.0001f);
  EXPECT_NEAR(1.866, mapped_bounding_box.Width(), 0.0001f);
  EXPECT_NEAR(2.232, mapped_bounding_box.Height(), 0.0001f);

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm_no_frame.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  rgm.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InnerDiv"), nullptr);
  rgm_no_frame.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "InnerDiv"), nullptr);
  EXPECT_EQ(PhysicalRect(21, 6, 1, 2),
            rgm.MapToAncestor(rect, GetFrameLayoutContainer(
                                        "test_frame", web_view, "CenterDiv")));
  EXPECT_EQ(
      PhysicalRect(21, 6, 1, 2),
      rgm_no_frame.MapToAncestor(
          rect, GetFrameLayoutContainer("test_frame", web_view, "CenterDiv")));

  rgm.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "OtherDiv"),
      GetFrameLayoutContainer("test_frame", web_view, "InnerDiv"));
  rgm_no_frame.PushMappingsToAncestor(
      GetFrameElement("test_frame", web_view, "OtherDiv"),
      GetFrameLayoutContainer("test_frame", web_view, "InnerDiv"));
  EXPECT_EQ(PhysicalRect(22, 12, 1, 2),
            rgm.MapToAncestor(rect, GetFrameLayoutContainer(
                                        "test_frame", web_view, "CenterDiv")));
  EXPECT_EQ(
      PhysicalRect(22, 12, 1, 2),
      rgm_no_frame.MapToAncestor(
          rect, GetFrameLayoutContainer("test_frame", web_view, "CenterDiv")));

  EXPECT_EQ(PhysicalRect(1, 6, 1, 2),
            rgm.MapToAncestor(rect, GetFrameLayoutContainer(
                                        "test_frame", web_view, "InnerDiv")));
  EXPECT_EQ(
      PhysicalRect(1, 6, 1, 2),
      rgm_no_frame.MapToAncestor(
          rect, GetFrameLayoutContainer("test_frame", web_view, "InnerDiv")));

  mapped_bounding_box = rgm.MapToAncestorQuad(rect, nullptr).BoundingBox();
  EXPECT_NEAR(87.8975f, mapped_bounding_box.X(), 0.0001f);
  EXPECT_NEAR(8.1532f, mapped_bounding_box.Y(), 0.0001f);
  EXPECT_NEAR(1.866, mapped_bounding_box.Width(), 0.0001f);
  EXPECT_NEAR(2.232, mapped_bounding_box.Height(), 0.0001f);

  EXPECT_EQ(PhysicalRect(50, 44, 1, 2),
            rgm_no_frame.MapToAncestor(rect, nullptr));
}

TEST_F(LayoutGeometryMapTest, ColumnTest) {
  RegisterMockedHttpURLLoad("rgm_column_test.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "rgm_column_test.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  // The document is 1000f wide (we resized to that size).
  // We have a 8px margin on either side of the document.
  // Between each column we have a 10px gap, and we have 3 columns.
  // The width of a given column is (1000 - 16 - 20)/3.
  // The total offset between any column and it's neighbour is width + 10px
  // (for the gap).
  float offset = (1000.0f - 16.0f - 20.0f) / 3.0f + 10.0f;

  LayoutGeometryMap rgm;
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "A"), nullptr);
  PhysicalRect rect(0, 0, 5, 3);

  EXPECT_EQ(PhysicalRect(8, 8, 5, 3), rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(PhysicalRect(0, 0, 5, 3), rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "Col1"), nullptr);
  EXPECT_EQ(PhysicalRect(8, 8, 5, 3), rgm.MapToAncestor(rect, nullptr));

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "Col2"), nullptr);
  PhysicalRect mapped_rect = rgm.MapToAncestor(rect, nullptr);
  EXPECT_NEAR(8.0f + offset, mapped_rect.X().ToFloat(), 0.1f);
  EXPECT_NEAR(8.0f, mapped_rect.Y().ToFloat(), 0.1f);
  EXPECT_EQ(PhysicalSize(5, 3), mapped_rect.size);

  rgm.PopMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm.PushMappingsToAncestor(GetLayoutBox(web_view, "Col3"), nullptr);
  mapped_rect = rgm.MapToAncestor(rect, nullptr);
  EXPECT_NEAR(8.0f + offset * 2.0f, mapped_rect.X().ToFloat(), 0.1f);
  EXPECT_NEAR(8.0f, mapped_rect.Y().ToFloat(), 0.1f);
  EXPECT_EQ(PhysicalSize(5, 3), mapped_rect.size);
}

TEST_F(LayoutGeometryMapTest, FloatUnderInlineLayer) {
  RegisterMockedHttpURLLoad("rgm_float_under_inline.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "rgm_float_under_inline.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  UpdateAllLifecyclePhases(web_view);

  LayoutGeometryMap rgm;
  auto* layer_under_float = GetLayoutBox(web_view, "layer-under-float");
  auto* span = GetElement(web_view, "span")->GetLayoutBoxModelObject();
  auto* floating = GetLayoutBox(web_view, "float");
  auto* container = GetLayoutBox(web_view, "container");
  PhysicalRect rect(3, 4, 10, 8);

  rgm.PushMappingsToAncestor(container->Layer(), nullptr);
  rgm.PushMappingsToAncestor(span->Layer(), container->Layer());
  rgm.PushMappingsToAncestor(layer_under_float->Layer(), span->Layer());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    // LayoutNG inline-level floats are children of their inline-level
    // containers. As such they are positioned relative to their inline-level
    // container, (and shifted by an additional 200,100 in this case).
    EXPECT_EQ(PhysicalRect(203, 104, 10, 8),
              rgm.MapToAncestor(rect, container));
    EXPECT_EQ(PhysicalRect(263, 154, 10, 8), rgm.MapToAncestor(rect, nullptr));
  } else {
    EXPECT_EQ(rect, rgm.MapToAncestor(rect, container));
    EXPECT_EQ(PhysicalRect(63, 54, 10, 8), rgm.MapToAncestor(rect, nullptr));
  }

  rgm.PopMappingsToAncestor(span->Layer());
  EXPECT_EQ(PhysicalRect(203, 104, 10, 8), rgm.MapToAncestor(rect, container));
  EXPECT_EQ(PhysicalRect(263, 154, 10, 8), rgm.MapToAncestor(rect, nullptr));

  rgm.PushMappingsToAncestor(floating, span);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(PhysicalRect(203, 104, 10, 8),
              rgm.MapToAncestor(rect, container));
    EXPECT_EQ(PhysicalRect(263, 154, 10, 8), rgm.MapToAncestor(rect, nullptr));
  } else {
    EXPECT_EQ(rect, rgm.MapToAncestor(rect, container));
    EXPECT_EQ(PhysicalRect(63, 54, 10, 8), rgm.MapToAncestor(rect, nullptr));
  }
}

}  // namespace blink
