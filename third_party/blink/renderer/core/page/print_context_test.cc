// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/print_context.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/before_print_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/skia/include/core/SkCanvas.h"

using testing::_;

namespace blink {

const int kPageWidth = 800;
const int kPageHeight = 600;

class MockPageContextCanvas : public SkCanvas {
 public:
  enum OperationType { kDrawRect, kDrawPoint };

  struct Operation {
    OperationType type;
    SkRect rect;
  };

  MockPageContextCanvas() : SkCanvas(kPageWidth, kPageHeight) {}
  ~MockPageContextCanvas() override = default;

  void onDrawAnnotation(const SkRect& rect,
                        const char key[],
                        SkData* value) override {
    if (rect.width() == 0 && rect.height() == 0) {
      SkPoint point = getTotalMatrix().mapXY(rect.x(), rect.y());
      Operation operation = {kDrawPoint,
                             SkRect::MakeXYWH(point.x(), point.y(), 0, 0)};
      recorded_operations_.push_back(operation);
    } else {
      Operation operation = {kDrawRect, rect};
      getTotalMatrix().mapRect(&operation.rect);
      recorded_operations_.push_back(operation);
    }
  }

  const Vector<Operation>& RecordedOperations() const {
    return recorded_operations_;
  }

  MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
  MOCK_METHOD1(DrawPicture, void(const SkPicture*));
  MOCK_METHOD1(OnDrawPicture, void(const SkPicture*));
  MOCK_METHOD3(OnDrawPicture,
               void(const SkPicture*, const SkMatrix*, const SkPaint*));
  MOCK_METHOD3(DrawPicture,
               void(const SkPicture*, const SkMatrix*, const SkPaint*));
  MOCK_METHOD4(onDrawImage,
               void(const SkImage*, SkScalar, SkScalar, const SkPaint*));
  MOCK_METHOD5(onDrawImageRect,
               void(const SkImage*,
                    const SkRect*,
                    const SkRect&,
                    const SkPaint*,
                    SrcRectConstraint));

 private:
  Vector<Operation> recorded_operations_;
};

class PrintContextTest : public PaintTestConfigurations, public RenderingTest {
 protected:
  explicit PrintContextTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client) {}
  ~PrintContextTest() override = default;

  void SetUp() override {
    RenderingTest::SetUp();
    print_context_ =
        MakeGarbageCollected<PrintContext>(GetDocument().GetFrame(),
                                           /*use_printing_layout=*/true);
  }

  PrintContext& GetPrintContext() { return *print_context_.Get(); }

  void SetBodyInnerHTML(String body_content) {
    GetDocument().body()->setAttribute(html_names::kStyleAttr, "margin: 0");
    GetDocument().body()->SetInnerHTMLFromString(body_content);
  }

  void PrintSinglePage(SkCanvas& canvas) {
    IntRect page_rect(0, 0, kPageWidth, kPageHeight);
    GetDocument().SetPrinting(Document::kBeforePrinting);
    Event* event = MakeGarbageCollected<BeforePrintEvent>();
    GetPrintContext().GetFrame()->DomWindow()->DispatchEvent(*event);
    GetPrintContext().BeginPrintMode(page_rect.Width(), page_rect.Height());
    UpdateAllLifecyclePhasesForTest();
    PaintRecordBuilder builder;
    GraphicsContext& context = builder.Context();
    context.SetPrinting(true);
    GetDocument().View()->PaintContentsOutsideOfLifecycle(
        context, kGlobalPaintPrinting | kGlobalPaintAddUrlMetadata,
        CullRect(page_rect));
    {
      DrawingRecorder recorder(
          context, *GetDocument().GetLayoutView(),
          DisplayItem::kPrintedContentDestinationLocations);
      GetPrintContext().OutputLinkedDestinations(context, page_rect);
    }
    builder.EndRecording()->Playback(&canvas);
    GetPrintContext().EndPrintMode();
  }

  static String AbsoluteBlockHtmlForLink(int x,
                                         int y,
                                         int width,
                                         int height,
                                         const char* url,
                                         const char* children = nullptr) {
    WTF::TextStream ts;
    ts << "<a style='position: absolute; left: " << x << "px; top: " << y
       << "px; width: " << width << "px; height: " << height << "px' href='"
       << url << "'>" << (children ? children : url) << "</a>";
    return ts.Release();
  }

  static String InlineHtmlForLink(const char* url,
                                  const char* children = nullptr) {
    WTF::TextStream ts;
    ts << "<a href='" << url << "'>" << (children ? children : url) << "</a>";
    return ts.Release();
  }

  static String HtmlForAnchor(int x,
                              int y,
                              const char* name,
                              const char* text_content) {
    WTF::TextStream ts;
    ts << "<a name='" << name << "' style='position: absolute; left: " << x
       << "px; top: " << y << "px'>" << text_content << "</a>";
    return ts.Release();
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<PrintContext> print_context_;
};

class PrintContextFrameTest : public PrintContextTest {
 public:
  PrintContextFrameTest()
      : PrintContextTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

#define EXPECT_SKRECT_EQ(expectedX, expectedY, expectedWidth, expectedHeight, \
                         actualRect)                                          \
  EXPECT_EQ(expectedX, actualRect.x());                                       \
  EXPECT_EQ(expectedY, actualRect.y());                                       \
  EXPECT_EQ(expectedWidth, actualRect.width());                               \
  EXPECT_EQ(expectedHeight, actualRect.height());

INSTANTIATE_PAINT_TEST_SUITE_P(PrintContextTest);

TEST_P(PrintContextTest, LinkTarget) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(150, 160, 170, 180,
                               "http://www.google.com#fragment"));
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderAnonymousBlockBeforeBlock) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<div style='padding-top: 50px'>" +
                   InlineHtmlForLink("http://www.google.com",
                                     "<img style='width: 111; height: 10'>") +
                   "<div> " +
                   InlineHtmlForLink("http://www.google1.com",
                                     "<img style='width: 122; height: 20'>") +
                   "</div>" + "</div>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 50, 111, 10, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 60, 122, 20, operations[1].rect);
}

TEST_P(PrintContextTest, LinkTargetContainingABlock) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<div style='padding-top: 50px'>" +
      InlineHtmlForLink("http://www.google2.com",
                        "<div style='width:133; height: 30'>BLOCK</div>") +
      "</div>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 50, 133, 30, operations[0].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderInInlines) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<span><b><i><img style='width: 40px; height: 40px'><br>" +
      InlineHtmlForLink("http://www.google3.com",
                        "<img style='width: 144px; height: 40px'>") +
      "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 40, 144, 40, operations[0].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderRelativelyPositionedInline) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
        + "<span style='position: relative; top: 50px; left: 50px'><b><i><img style='width: 1px; height: 40px'><br>"
        + InlineHtmlForLink("http://www.google3.com", "<img style='width: 155px; height: 50px'>")
        + "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 90, 155, 50, operations[0].rect);
}

TEST_P(PrintContextTest, LinkTargetSvg) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(R"HTML(
    <svg width='100' height='100'>
    <a xlink:href='http://www.w3.org'><rect x='20' y='20' width='50'
    height='50'/></a>
    <text x='10' y='90'><a
    xlink:href='http://www.google.com'><tspan>google</tspan></a></text>
    </svg>
  )HTML");
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(20, 20, 50, 50, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_EQ(10, operations[1].rect.x());
  EXPECT_GE(90, operations[1].rect.y());
}

TEST_P(PrintContextTest, LinkedTarget) {
  MockPageContextCanvas canvas;
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(
      AbsoluteBlockHtmlForLink(
          50, 60, 70, 80,
          "#fragment")  // Generates a Link_Named_Dest_Key annotation
      + AbsoluteBlockHtmlForLink(150, 160, 170, 180,
                                 "#not-found")  // Generates no annotation
      +
      HtmlForAnchor(250, 260, "fragment",
                    "fragment")  // Generates a Define_Named_Dest_Key annotation
      + HtmlForAnchor(350, 360, "fragment-not-used",
                      "fragment-not-used"));  // Generates no annotation
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[1].type);
  EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_P(PrintContextTest, EmptyLinkedTarget) {
  MockPageContextCanvas canvas;
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(AbsoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment") +
                   HtmlForAnchor(250, 260, "fragment", ""));
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[1].type);
  EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_P(PrintContextTest, LinkTargetBoundingBox) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      AbsoluteBlockHtmlForLink(50, 60, 70, 20, "http://www.google.com",
                               "<img style='width: 200px; height: 100px'>"));
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 200, 100, operations[0].rect);
}

INSTANTIATE_PAINT_TEST_SUITE_P(PrintContextFrameTest);

TEST_P(PrintContextFrameTest, WithSubframe) {
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { display: none }</style>
    <iframe src='http://b.com/' width='500' height='500'
     style='border-width: 5px; margin: 5px; position: absolute; top: 90px;
    left: 90px'></iframe>
  )HTML");
  SetChildFrameHTML(
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment") +
      AbsoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(250, 260, 270, 280,
                               "http://www.google.com#fragment"));

  MockPageContextCanvas canvas;
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(250, 260, 170, 180, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(350, 360, 270, 280, operations[1].rect);
}

TEST_P(PrintContextFrameTest, WithScrolledSubframe) {
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { display: none }</style>
    <iframe src='http://b.com/' width='500' height='500'
     style='border-width: 5px; margin: 5px; position: absolute; top: 90px;
    left: 90px'></iframe>
  )HTML");
  SetChildFrameHTML(
      AbsoluteBlockHtmlForLink(10, 10, 20, 20, "http://invisible.com") +
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "http://partly.visible.com") +
      AbsoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(250, 260, 270, 280,
                               "http://www.google.com#fragment") +
      AbsoluteBlockHtmlForLink(850, 860, 70, 80,
                               "http://another.invisible.com"));

  ChildDocument().domWindow()->scrollTo(100, 100);

  MockPageContextCanvas canvas;
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(3u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80,
                   operations[0].rect);  // FIXME: the rect should be clipped.
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(250, 260, 270, 280, operations[2].rect);
}

// This tests that we properly resize and re-layout pages for printing.
TEST_P(PrintContextFrameTest, BasicPrintPageLayout) {
  FloatSize page_size(400, 400);
  float maximum_shrink_ratio = 1.1;
  auto* node = GetDocument().documentElement();

  GetDocument().GetFrame()->StartPrinting(page_size, page_size,
                                          maximum_shrink_ratio);
  EXPECT_EQ(node->OffsetWidth(), 400);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(node->OffsetWidth(), 800);

  SetBodyInnerHTML(R"HTML(
      <div style='border: 0px; margin: 0px; background-color: #0000FF;
      width:800px; height:400px'></div>)HTML");
  GetDocument().GetFrame()->StartPrinting(page_size, page_size,
                                          maximum_shrink_ratio);
  EXPECT_EQ(node->OffsetWidth(), 440);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(node->OffsetWidth(), 800);
}

TEST_P(PrintContextTest, Canvas2DBeforePrint) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<canvas id='c' width=100 height=100></canvas>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element->setTextContent(
      "window.addEventListener('beforeprint', (ev) => {"
      "const ctx = document.getElementById('c').getContext('2d');"
      "ctx.fillRect(0, 0, 10, 10);"
      "ctx.fillRect(50, 50, 10, 10);"
      "});");
  GetDocument().body()->AppendChild(script_element);

  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(testing::AtLeast(2));

  PrintSinglePage(canvas);
}

TEST_P(PrintContextTest, Canvas2DPixelated) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<canvas id='c' style='image-rendering: pixelated' "
      "width=100 height=100></canvas>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element->setTextContent(
      "window.addEventListener('beforeprint', (ev) => {"
      "const ctx = document.getElementById('c').getContext('2d');"
      "ctx.fillRect(0, 0, 10, 10);"
      "ctx.fillRect(50, 50, 10, 10);"
      "});");
  GetDocument().body()->AppendChild(script_element);

  EXPECT_CALL(canvas, onDrawImageRect(_, _, _, _, _));

  PrintSinglePage(canvas);
}

// This tests that we don't resize or re-layout subframes in printed content.
// TODO(weili): This test fails when the iframe isn't the root scroller - e.g.
// Adding ScopedImplicitRootScrollerForTest disabler(false);
// https://crbug.com/841602.
TEST_P(PrintContextFrameTest, DISABLED_SubframePrintPageLayout) {
  SetBodyInnerHTML(R"HTML(
      <div style='border: 0px; margin: 0px; background-color: #0000FF;
      width:800px; height:400px'></div>
      <iframe id="target" src='http://b.com/' width='100%' height='100%'
      style='border: 0px; margin: 0px; position: absolute; top: 0px;
      left: 0px'></iframe>)HTML");
  FloatSize page_size(400, 400);
  float maximum_shrink_ratio = 1.1;
  auto* parent = GetDocument().documentElement();
  // The child document element inside iframe.
  auto* child = ChildDocument().documentElement();
  // The iframe element in the document.
  auto* target = GetDocument().getElementById("target");

  GetDocument().GetFrame()->StartPrinting(page_size, page_size,
                                          maximum_shrink_ratio);
  EXPECT_EQ(parent->OffsetWidth(), 440);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 440);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);

  GetDocument().GetFrame()->StartPrinting();
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);

  ASSERT_TRUE(ChildDocument() != GetDocument());
  ChildDocument().GetFrame()->StartPrinting(page_size, page_size,
                                            maximum_shrink_ratio);
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 400);
  EXPECT_EQ(target->OffsetWidth(), 800);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(parent->OffsetWidth(), 800);
  //  The child frame should return to the original size.
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);
}

}  // namespace blink
