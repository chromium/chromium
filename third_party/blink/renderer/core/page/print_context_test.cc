// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/page/print_context.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/before_print_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
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
    // Ignore PDF node key annotations, defined in SkPDFDocument.cpp.
    if (0 == strcmp(key, "PDF_Node_Key"))
      return;

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
  MOCK_METHOD3(onDrawPicture,
               void(const SkPicture*, const SkMatrix*, const SkPaint*));
  MOCK_METHOD5(onDrawImage2,
               void(const SkImage*,
                    SkScalar,
                    SkScalar,
                    const SkSamplingOptions&,
                    const SkPaint*));
  MOCK_METHOD6(onDrawImageRect2,
               void(const SkImage*,
                    const SkRect&,
                    const SkRect&,
                    const SkSamplingOptions&,
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
        MakeGarbageCollected<PrintContext>(GetDocument().GetFrame());
    base::FieldTrialParams auto_flush_params;
    auto_flush_params["max_pinned_image_kb"] = "1";
    print_feature_list_.InitAndEnableFeatureWithParameters(
        kCanvas2DAutoFlushParams, auto_flush_params);
  }

  void TearDown() override {
    RenderingTest::TearDown();
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
    print_feature_list_.Reset();
  }

  PrintContext& GetPrintContext() { return *print_context_.Get(); }

  void SetBodyInnerHTML(String body_content) {
    GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                       AtomicString("margin: 0"));
    GetDocument().body()->setInnerHTML(body_content);
  }

  gfx::Rect PrintSinglePage(SkCanvas& canvas, int page_index = 0) {
    GetDocument().SetPrinting(Document::kBeforePrinting);
    Event* event = MakeGarbageCollected<BeforePrintEvent>();
    GetPrintContext().GetFrame()->DomWindow()->DispatchEvent(*event);
    GetPrintContext().BeginPrintMode(
        WebPrintParams(gfx::SizeF(kPageWidth, kPageHeight)));
    GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kTest);

    gfx::Rect page_rect = GetPrintContext().PageRect(page_index);

    PaintRecordBuilder builder;
    GraphicsContext& context = builder.Context();
    context.SetPrinting(true);
    GetDocument().View()->PrintPage(context, page_index, CullRect(page_rect));
    GetPrintContext().OutputLinkedDestinations(
        context,
        GetDocument().GetLayoutView()->FirstFragment().ContentsProperties(),
        page_rect);
    builder.EndRecording().Playback(&canvas);
    GetPrintContext().EndPrintMode();

    // The drawing operations are relative to the current page.
    return gfx::Rect(page_rect.size());
  }

  static String AbsoluteBlockHtmlForLink(int x,
                                         int y,
                                         int width,
                                         int height,
                                         String url,
                                         String children = String()) {
    WTF::TextStream ts;
    ts << "<a style='position: absolute; left: " << x << "px; top: " << y
       << "px; width: " << width << "px; height: " << height << "px' href='"
       << url << "'>" << (children ? children : url) << "</a>";
    return ts.Release();
  }

  static String InlineHtmlForLink(String url, String children = String()) {
    WTF::TextStream ts;
    ts << "<a href='" << url << "'>" << (children ? children : url) << "</a>";
    return ts.Release();
  }

  static String HtmlForAnchor(int x, int y, String name, String text_content) {
    WTF::TextStream ts;
    ts << "<a name='" << name << "' style='position: absolute; left: " << x
       << "px; top: " << y << "px'>" << text_content << "</a>";
    return ts.Release();
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<PrintContext> print_context_;
  base::test::ScopedFeatureList print_feature_list_;
};

class PrintContextFrameTest : public PrintContextTest {
 public:
  PrintContextFrameTest()
      : PrintContextTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

#define EXPECT_SKRECT_EQ(expectedX, expectedY, expectedWidth, expectedHeight, \
                         actualRect)                                          \
  do {                                                                        \
    EXPECT_EQ(expectedX, actualRect.x());                                     \
    EXPECT_EQ(expectedY, actualRect.y());                                     \
    EXPECT_EQ(expectedWidth, actualRect.width());                             \
    EXPECT_EQ(expectedHeight, actualRect.height());                           \
  } while (false)

INSTANTIATE_PAINT_TEST_SUITE_P(PrintContextTest);

TEST_P(PrintContextTest, LinkTarget) {
  testing::NiceMock<MockPageContextCanvas> canvas;
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

TEST_P(PrintContextTest, LinkTargetInCompositedScroller) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
      "<div style='width: 200px; height: 200px; overflow: scroll;"
      "            position: relative; will-change: scroll-position'>" +
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(250, 60, 70, 80, "http://www.google.com") +
      "</div>");
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderAnonymousBlockBeforeBlock) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  testing::NiceMock<MockPageContextCanvas> canvas;
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
  ASSERT_EQ(4u, operations.size());
  // First 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 59, 111, 1, operations[0].rect);
  // First image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 50, 111, 10, operations[1].rect);
  // Second 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(0, 79, 122, 1, operations[2].rect);
  // Second image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[3].type);
  EXPECT_SKRECT_EQ(0, 60, 122, 20, operations[3].rect);
}

TEST_P(PrintContextTest, LinkTargetContainingABlock) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
      "<div style='padding-top: 50px; width:555px;'>" +
      InlineHtmlForLink("http://www.google2.com",
                        "<div style='width:133px; height: 30px'>BLOCK</div>") +
      "</div>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(5u, operations.size());
  // Empty line before the line with the block inside:
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[0].type);
  EXPECT_SKRECT_EQ(0, 50, 0, 0, operations[0].rect);
  // The line with the block inside:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 50, 555, 30, operations[1].rect);
  // Empty line after the line with the block inside:
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[2].type);
  EXPECT_SKRECT_EQ(0, 80, 0, 0, operations[2].rect);
  // The block:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[3].type);
  EXPECT_SKRECT_EQ(0, 50, 133, 30, operations[3].rect);
  // The line inside the block (with the text "BLOCK") (we cannot reliably test
  // the size of this rectangle, as it varies across platforms):
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[4].type);
}

TEST_P(PrintContextTest, LinkTargetUnderInInlines) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
      "<span><b><i><img style='width: 40px; height: 40px'><br>" +
      InlineHtmlForLink("http://www.google3.com",
                        "<img style='width: 144px; height: 40px'>") +
      "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  // The 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 79, 144, 1, operations[0].rect);
  // The image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 40, 144, 40, operations[1].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderInInlinesMultipleLines) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
      "<span><b><i><img style='width: 40px; height: 40px'><br>" +
      InlineHtmlForLink("http://www.google3.com",
                        "<img style='width: 144px; height: 40px'><br><img "
                        "style='width: 14px; height: 40px'>") +
      "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(4u, operations.size());
  // The 'A' element on the second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 79, 144, 1, operations[0].rect);
  // The 'A' element on the third line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 119, 14, 1, operations[1].rect);
  // The second image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(0, 40, 144, 40, operations[2].rect);
  // The third image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[3].type);
  EXPECT_SKRECT_EQ(0, 80, 14, 40, operations[3].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderInInlinesMultipleLinesCulledInline) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML("<span><b><i><br>" +
                   InlineHtmlForLink("http://www.google3.com", "xxx<br>xxx") +
                   "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(3u, operations.size());
  // In this test, only check that we have rectangles. We cannot reliably test
  // their size, since it varies across platforms.
  //
  // Second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  // Newline at the end of the second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  // Third line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
}

TEST_P(PrintContextTest, LinkTargetRelativelyPositionedInline) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<a style='position: relative; top: 50px; left: 50px' "
      "href='http://www.google3.com'>"
      "  <img style='width: 1px; height: 40px'>"
      "</a>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  // The 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 89, 1, 1, operations[0].rect);
  // The image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(50, 50, 1, 40, operations[1].rect);
}

TEST_P(PrintContextTest, LinkTargetUnderRelativelyPositionedInline) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
        + "<span style='position: relative; top: 50px; left: 50px'><b><i><img style='width: 1px; height: 40px'><br>"
        + InlineHtmlForLink("http://www.google3.com", "<img style='width: 155px; height: 50px'>")
        + "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  // The 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 139, 155, 1, operations[0].rect);
  // The image:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(50, 90, 155, 50, operations[1].rect);
}

TEST_P(PrintContextTest,
       LinkTargetUnderRelativelyPositionedInlineMultipleLines) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
        + "<span style='position: relative; top: 50px; left: 50px'><b><i><img style='width: 1px; height: 40px'><br>"
        + InlineHtmlForLink(
            "http://www.google3.com",
            "<img style='width: 10px; height: 50px'><br><img style='width: 155px; height: 50px'>")
        + "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(4u, operations.size());
  // The 'A' element on the second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 139, 10, 1, operations[0].rect);
  // The 'A' element on the third line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(50, 189, 155, 1, operations[1].rect);
  // The image on the second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(50, 90, 10, 50, operations[2].rect);
  // The image on the third line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[3].type);
  EXPECT_SKRECT_EQ(50, 140, 155, 50, operations[3].rect);
}

TEST_P(PrintContextTest,
       LinkTargetUnderRelativelyPositionedInlineMultipleLinesCulledInline) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(
      +"<span style='position: relative; top: 50px; left: 50px'><b><i><br>" +
      InlineHtmlForLink("http://www.google3.com", "xxx<br>xxx") +
      "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(3u, operations.size());
  // In this test, only check that we have rectangles. We cannot reliably test
  // their size, since it varies across platforms.
  //
  // Second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  // Newline at end of second line.
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  // Third line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
}

TEST_P(PrintContextTest, SingleLineLinkNextToWrappedLink) {
  testing::NiceMock<MockPageContextCanvas> canvas;
  SetBodyInnerHTML(R"HTML(
    <div style="width:120px;">
      <a href="http://www.google.com/">
        <img style="width:50px; height:20px;">
      </a>
      <a href="http://www.google.com/maps/">
        <img style="width:50px; height:20px;">
        <img style="width:60px; height:20px;">
      </a>
    </div>
  )HTML");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(6u, operations.size());
  // First 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 19, 50, 1, operations[0].rect);
  // Image inside first 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 0, 50, 20, operations[1].rect);
  // Second 'A' element on the first line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(50, 19, 50, 1, operations[2].rect);
  // Second 'A' element on the second line:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[3].type);
  EXPECT_SKRECT_EQ(0, 39, 60, 1, operations[3].rect);
  // First image in the second 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[4].type);
  EXPECT_SKRECT_EQ(50, 0, 50, 20, operations[4].rect);
  // Second image in the second 'A' element:
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[5].type);
  EXPECT_SKRECT_EQ(0, 20, 60, 20, operations[5].rect);
}

TEST_P(PrintContextTest, LinkTargetSvg) {
  testing::NiceMock<MockPageContextCanvas> canvas;
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
  testing::NiceMock<MockPageContextCanvas> canvas;
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  // Careful about locations, the page is 800x600 and only one page is printed.
  SetBodyInnerHTML(
      // Generates a Link_Named_Dest_Key annotation.
      AbsoluteBlockHtmlForLink(50, 60, 10, 10, "#fragment") +
      // Generates no annotation.
      AbsoluteBlockHtmlForLink(50, 160, 10, 10, "#not-found") +
      // Generates a Link_Named_Dest_Key annotation.
      AbsoluteBlockHtmlForLink(50, 260, 10, 10, u"#\u00F6") +
      // Generates a Link_Named_Dest_Key annotation.
      AbsoluteBlockHtmlForLink(50, 360, 10, 10, "#") +
      // Generates a Link_Named_Dest_Key annotation.
      AbsoluteBlockHtmlForLink(50, 460, 10, 10, "#t%6Fp") +
      // Generates a Define_Named_Dest_Key annotation.
      HtmlForAnchor(450, 60, "fragment", "fragment") +
      // Generates no annotation.
      HtmlForAnchor(450, 160, "fragment-not-used", "fragment-not-used")
      // Generates a Define_Named_Dest_Key annotation.
      + HtmlForAnchor(450, 260, u"\u00F6", "O")
      // TODO(1117212): The escaped version currently takes precedence.
      // Generates a Define_Named_Dest_Key annotation.
      //+ HtmlForAnchor(450, 360, "%C3%B6", "O2")
  );
  PrintSinglePage(canvas);

  Vector<MockPageContextCanvas::Operation> operations =
      canvas.RecordedOperations();
  ASSERT_EQ(8u, operations.size());
  // The DrawRect operations come from a stable iterator.
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 10, 10, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(50, 260, 10, 10, operations[1].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(50, 360, 10, 10, operations[2].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[3].type);
  EXPECT_SKRECT_EQ(50, 460, 10, 10, operations[3].rect);

  // The DrawPoint operations come from an unstable iterator.
  std::sort(operations.begin() + 4, operations.begin() + 8,
            [](const MockPageContextCanvas::Operation& a,
               const MockPageContextCanvas::Operation& b) {
              return std::pair(a.rect.x(), a.rect.y()) <
                     std::pair(b.rect.x(), b.rect.y());
            });
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[4].type);
  EXPECT_SKRECT_EQ(0, 0, 0, 0, operations[4].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[5].type);
  EXPECT_SKRECT_EQ(0, 0, 0, 0, operations[5].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[6].type);
  EXPECT_SKRECT_EQ(450, 60, 0, 0, operations[6].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[7].type);
  EXPECT_SKRECT_EQ(450, 260, 0, 0, operations[7].rect);
}

TEST_P(PrintContextTest, EmptyLinkedTarget) {
  testing::NiceMock<MockPageContextCanvas> canvas;
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
  testing::NiceMock<MockPageContextCanvas> canvas;
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

TEST_P(PrintContextTest, LinkInFragmentedContainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
        line-height: 50px;
        orphans: 1;
        widows: 1;
      }
    </style>
    <div style="height:calc(100vh - 90px);"></div>
    <div>
      <a href="http://www.google.com">link 1</a><br>
      <!-- Page break here. -->
      <a href="http://www.google.com">link 2</a><br>
      <a href="http://www.google.com">link 3</a><br>
    </div>
  )HTML");

  testing::NiceMock<MockPageContextCanvas> first_page_canvas;
  gfx::Rect page_rect = PrintSinglePage(first_page_canvas, 0);
  Vector<MockPageContextCanvas::Operation> operations =
      first_page_canvas.RecordedOperations();

  // TODO(crbug.com/1392701): Should be 1.
  ASSERT_EQ(operations.size(), 3u);

  const auto& page1_link1 = operations[0];
  EXPECT_EQ(page1_link1.type, MockPageContextCanvas::kDrawRect);
  EXPECT_GE(page1_link1.rect.y(), page_rect.height() - 90);
  EXPECT_LE(page1_link1.rect.bottom(), page_rect.height() - 40);

  testing::NiceMock<MockPageContextCanvas> second_page_canvas;
  page_rect = PrintSinglePage(second_page_canvas, 1);
  operations = second_page_canvas.RecordedOperations();

  // TODO(crbug.com/1392701): Should be 2.
  ASSERT_EQ(operations.size(), 3u);
  // TODO(crbug.com/1392701): Should be operations[0]
  const auto& page2_link1 = operations[1];
  // TODO(crbug.com/1392701): Should be operations[1]
  const auto& page2_link2 = operations[2];

  EXPECT_EQ(page2_link1.type, MockPageContextCanvas::kDrawRect);
  EXPECT_GE(page2_link1.rect.y(), page_rect.y());
  EXPECT_LE(page2_link1.rect.bottom(), page_rect.y() + 50);
  EXPECT_EQ(page2_link2.type, MockPageContextCanvas::kDrawRect);
  EXPECT_GE(page2_link2.rect.y(), page_rect.y() + 50);
  EXPECT_LE(page2_link2.rect.bottom(), page_rect.y() + 100);
}

TEST_P(PrintContextTest, LinkedTargetSecondPage) {
  SetBodyInnerHTML(R"HTML(
    <a style="display:block; width:33px; height:33px;" href="#nextpage"></a>
    <div style="break-before:page;"></div>
    <div id="nextpage" style="margin-top:50px; width:100px; height:100px;"></div>
  )HTML");

  // The link is on the first page.
  testing::NiceMock<MockPageContextCanvas> first_canvas;
  PrintSinglePage(first_canvas, 0);
  const Vector<MockPageContextCanvas::Operation>* operations =
      &first_canvas.RecordedOperations();
  ASSERT_EQ(1u, operations->size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, (*operations)[0].type);
  EXPECT_SKRECT_EQ(0, 0, 33, 33, (*operations)[0].rect);

  // The destination is on the second page.
  testing::NiceMock<MockPageContextCanvas> second_canvas;
  PrintSinglePage(second_canvas, 1);
  operations = &second_canvas.RecordedOperations();
  ASSERT_EQ(1u, operations->size());
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, (*operations)[0].type);
  EXPECT_SKRECT_EQ(0, 50, 0, 0, (*operations)[0].rect);
}

// Here are a few tests to check that shrink to fit doesn't mess up page count.

TEST_P(PrintContextTest, ScaledVerticalRL1) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:vertical-rl; }</style>
    <div style="break-after:page;">x</div>
    <div style="inline-size:10000px; block-size:10px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(2, page_count);
}

TEST_P(PrintContextTest, ScaledVerticalRL2) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:vertical-rl; }</style>
    <div style="break-after:page;">x</div>
    <div style="inline-size:10000px; block-size:500px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(2, page_count);
}

TEST_P(PrintContextTest, ScaledVerticalRL3) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:vertical-rl; }</style>
    <div style="break-after:page;">x</div>
    <div style="break-after:page; inline-size:10000px; block-size:10px;"></div>
    <div style="inline-size:10000px; block-size:10px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(3, page_count);
}

TEST_P(PrintContextTest, ScaledVerticalLR1) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:vertical-lr; }</style>
    <div style="break-after:page;">x</div>
    <div style="inline-size:10000px; block-size:10px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(2, page_count);
}

TEST_P(PrintContextTest, ScaledVerticalLR2) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:vertical-lr; }</style>
    <div style="break-after:page;">x</div>
    <div style="inline-size:10000px; block-size:500px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(2, page_count);
}

TEST_P(PrintContextTest, ScaledVerticalLR3) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:vertical-lr; }</style>
    <div style="break-after:page;">x</div>
    <div style="break-after:page; inline-size:10000px; block-size:10px;"></div>
    <div style="inline-size:10000px; block-size:10px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(3, page_count);
}

TEST_P(PrintContextTest, ScaledHorizontalTB1) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:horizontal-tb; }</style>
    <div style="break-after:page;">x</div>
    <div style="inline-size:10000px; block-size:10px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(2, page_count);
}

TEST_P(PrintContextTest, ScaledHorizontalTB2) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:horizontal-tb; }</style>
    <div style="break-after:page;">x</div>
    <div style="inline-size:10000px; block-size:500px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(2, page_count);
}

TEST_P(PrintContextTest, ScaledHorizontalTB3) {
  SetBodyInnerHTML(R"HTML(
    <style>html { writing-mode:horizontal-tb; }</style>
    <div style="break-after:page;">x</div>
    <div style="break-after:page; inline-size:10000px; block-size:10px;"></div>
    <div style="inline-size:10000px; block-size:10px;"></div>
  )HTML");

  int page_count = PrintContext::NumberOfPages(GetDocument().GetFrame(),
                                               gfx::SizeF(500, 500));
  EXPECT_EQ(3, page_count);
}

TEST_P(PrintContextTest, SvgMarkersOnMultiplePages) {
  SetBodyInnerHTML(R"HTML(
    <style>
      svg {
        display: block;
      }
    </style>
    <svg style="break-after: page">
      <marker id="m1" markerUnits="userSpaceOnUse" overflow="visible">
        <rect width="100" height="75" transform="translate(1,0)"/>
      </marker>
      <path d="M0,0h1" marker-start="url(#m1)"/>
    </svg>
    <svg>
      <marker id="m2" markerUnits="userSpaceOnUse" overflow="visible">
        <rect width="50" height="25" transform="translate(2,0)"/>
      </marker>
      <path d="M0,0h1" marker-start="url(#m2)"/>
    </svg>
  )HTML");

  class MockCanvas : public SkCanvas {
   public:
    MockCanvas() : SkCanvas(kPageWidth, kPageHeight) {}

    MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
    MOCK_METHOD2(didTranslate, void(SkScalar, SkScalar));
  };

  MockCanvas first_page_canvas;
  EXPECT_CALL(first_page_canvas, didTranslate(1, 0)).Times(1);
  EXPECT_CALL(first_page_canvas, onDrawRect(SkRect::MakeWH(100, 75), _))
      .Times(1);
  PrintSinglePage(first_page_canvas, 0);

  MockCanvas second_page_canvas;
  EXPECT_CALL(second_page_canvas, didTranslate(2, 0)).Times(1);
  EXPECT_CALL(second_page_canvas, onDrawRect(SkRect::MakeWH(50, 25), _))
      .Times(1);
  PrintSinglePage(second_page_canvas, 1);
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
  gfx::SizeF page_size(400, 400);
  float maximum_shrink_ratio = 1.1;
  auto* node = GetDocument().documentElement();

  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size),
                                          maximum_shrink_ratio);
  EXPECT_EQ(node->OffsetWidth(), 400);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(node->OffsetWidth(), 800);

  SetBodyInnerHTML(R"HTML(
      <div style='border: 0px; margin: 0px; background-color: #0000FF;
      width:800px; height:400px'></div>)HTML");
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size),
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

  EXPECT_CALL(canvas, onDrawImageRect2(_, _, _, _, _, _));

  PrintSinglePage(canvas);
}

TEST_P(PrintContextTest, Canvas2DAutoFlushingSuppressed) {
  // When printing, we're supposed to make a best effore to avoid flushing
  // a canvas's PaintOps in order to support vector printing whenever possible.
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<canvas id='c' width=200 height=100></canvas>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  // Note: source_canvas is 10x10, which consumes 400 bytes for pixel data,
  // which is larger than the 100 limit set in PrintContextTest::SetUp().
  script_element->setTextContent(
      "source_canvas = document.createElement('canvas');"
      "source_canvas.width = 10;"
      "source_canvas.height = 10;"
      "source_ctx = source_canvas.getContext('2d');"
      "source_ctx.fillRect(1000, 0, 1, 1);"
      "window.addEventListener('beforeprint', (ev) => {"
      "  ctx = document.getElementById('c').getContext('2d');"
      "  ctx.fillStyle = 'green';"
      "  ctx.fillRect(0, 0, 100, 100);"
      "  ctx.drawImage(source_canvas, 101, 0);"
      // Next op normally triggers an auto-flush due to exceeded memory limit
      // but in this case, the auto-flush is suppressed.
      "  ctx.fillRect(0, 0, 1, 1);"
      "});");
  GetDocument().body()->AppendChild(script_element);

  // Verify that the auto-flush was suppressed by checking that the first
  // fillRect call flowed through to 'canvas'.
  testing::Sequence s;
  // The first fillRect call
  EXPECT_CALL(canvas, onDrawRect(_, _))
      .Times(testing::Exactly(1))
      .InSequence(s);
  // The drawImage call
  EXPECT_CALL(canvas, onDrawImageRect2(_, _, _, _, _, _)).InSequence(s);
  // The secondFillRect
  EXPECT_CALL(canvas, onDrawRect(_, _)).InSequence(s);

  PrintSinglePage(canvas);
}

// For testing printing behavior when 2d canvases are gpu-accelerated.
class PrintContextAcceleratedCanvasTest : public PrintContextTest {
 public:
  void SetUp() override {
    accelerated_canvas_scope_ =
        std::make_unique<ScopedAccelerated2dCanvasForTest>(true);
    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContextGLES2(test_context_provider_.get());

    PrintContextTest::SetUp();

    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
  }

  void TearDown() override {
    // Call base class TeardDown first to ensure Canvas2DLayerBridge is
    // destroyed before the TestContextProvider.
    PrintContextTest::TearDown();

    SharedGpuContext::Reset();
    test_context_provider_ = nullptr;
    accelerated_canvas_scope_ = nullptr;
  }

 private:
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  std::unique_ptr<ScopedAccelerated2dCanvasForTest> accelerated_canvas_scope_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(PrintContextAcceleratedCanvasTest);

TEST_P(PrintContextAcceleratedCanvasTest, Canvas2DBeforePrint) {
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

  // 2 fillRects.
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(testing::Exactly(2));

  PrintSinglePage(canvas);
}

namespace {

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

}  // namespace

// For testing printing behavior when 2d canvas contexts use oop rasterization.
class PrintContextOOPRCanvasTest : public PrintContextTest {
 public:
  void SetUp() override {
    accelerated_canvas_scope_ =
        std::make_unique<ScopedAccelerated2dCanvasForTest>(true);
    std::unique_ptr<viz::TestGLES2Interface> gl_context =
        std::make_unique<viz::TestGLES2Interface>();
    gl_context->set_gpu_rasterization(true);
    std::unique_ptr<viz::TestContextSupport> context_support =
        std::make_unique<viz::TestContextSupport>();
    std::unique_ptr<viz::TestRasterInterface> raster_interface =
        std::make_unique<viz::TestRasterInterface>();
    test_context_provider_ = base::MakeRefCounted<viz::TestContextProvider>(
        std::move(context_support), std::move(gl_context),
        std::move(raster_interface),
        /*shared_image_interface=*/nullptr,
        /*support_locking=*/false);

    InitializeSharedGpuContextGLES2(test_context_provider_.get());

    PrintContextTest::SetUp();
    accelerated_compositing_scope_ = std::make_unique<
        ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>();

    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
  }

  void TearDown() override {
    // Call base class TeardDown first to ensure Canvas2DLayerBridge is
    // destroyed before the TestContextProvider.
    accelerated_compositing_scope_ = nullptr;
    test_context_provider_ = nullptr;
    SharedGpuContext::Reset();
    PrintContextTest::TearDown();
    accelerated_canvas_scope_ = nullptr;
  }

 private:
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  std::unique_ptr<ScopedAccelerated2dCanvasForTest> accelerated_canvas_scope_;
  std::unique_ptr<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>
      accelerated_compositing_scope_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(PrintContextOOPRCanvasTest);

TEST_P(PrintContextOOPRCanvasTest, Canvas2DBeforePrint) {
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

  // 2 fillRects.
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(testing::Exactly(2));

  PrintSinglePage(canvas);
}

TEST_P(PrintContextOOPRCanvasTest, Canvas2DFlushForImageListener) {
  base::test::ScopedFeatureList feature_list_;
  // Verifies that a flush triggered by a change to a source canvas results
  // in printing falling out of vector print mode.

  // This test needs to run with CanvasOopRasterization enabled in order to
  // exercise the FlushForImageListener code path in CanvasResourceProvider.
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<canvas id='c' width=200 height=100></canvas>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element->setTextContent(
      "source_canvas = document.createElement('canvas');"
      "source_canvas.width = 5;"
      "source_canvas.height = 5;"
      "source_ctx = source_canvas.getContext('2d', {willReadFrequently: "
      "'false'});"
      "source_ctx.fillRect(0, 0, 1, 1);"
      "image_data = source_ctx.getImageData(0, 0, 5, 5);"
      "window.addEventListener('beforeprint', (ev) => {"
      "  ctx = document.getElementById('c').getContext('2d');"
      "  ctx.drawImage(source_canvas, 0, 0);"
      // Touching source_ctx forces a flush of both contexts, which cancels
      // vector printing.
      "  source_ctx.putImageData(image_data, 0, 0);"
      "  ctx.fillRect(0, 0, 1, 1);"
      "});");
  GetDocument().body()->AppendChild(script_element);

  // Verify that the auto-flush caused the canvas printing to fall out of
  // vector mode.
  testing::Sequence s;
  // The bitmap blit
  EXPECT_CALL(canvas, onDrawImageRect2(_, _, _, _, _, _)).InSequence(s);
  // The fill rect in the event listener should leave no trace here because
  // it is supposed to be included in the canvas blit.
  EXPECT_CALL(canvas, onDrawRect(_, _))
      .Times(testing::Exactly(0))
      .InSequence(s);

  PrintSinglePage(canvas);
}

TEST_P(PrintContextOOPRCanvasTest, Canvas2DNoFlushForImageListener) {
  // Verifies that a the canvas printing stays in vector mode after a
  // canvas to canvas drawImage, as long as the source canvas is not
  // touched afterwards.
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<canvas id='c' width=200 height=100></canvas>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element->setTextContent(
      "source_canvas = document.createElement('canvas');"
      "source_canvas.width = 5;"
      "source_canvas.height = 5;"
      "source_ctx = source_canvas.getContext('2d');"
      "source_ctx.fillRect(0, 0, 1, 1);"
      "window.addEventListener('beforeprint', (ev) => {"
      "  ctx = document.getElementById('c').getContext('2d');"
      "  ctx.fillStyle = 'green';"
      "  ctx.fillRect(0, 0, 100, 100);"
      "  ctx.drawImage(source_canvas, 0, 0, 5, 5, 101, 0, 10, 10);"
      "  ctx.fillRect(0, 0, 1, 1);"
      "});");
  GetDocument().body()->AppendChild(script_element);

  // Verify that the auto-flush caused the canvas printing to fall out of
  // vector mode.
  testing::Sequence s;
  // The fillRect call
  EXPECT_CALL(canvas, onDrawRect(_, _))
      .Times(testing::Exactly(1))
      .InSequence(s);
  // The drawImage
  EXPECT_CALL(canvas, onDrawImageRect2(_, _, _, _, _, _)).InSequence(s);
  // The fill rect after the drawImage
  EXPECT_CALL(canvas, onDrawRect(_, _))
      .Times(testing::Exactly(1))
      .InSequence(s);

  PrintSinglePage(canvas);
}

TEST_P(PrintContextTest, Canvas2DAutoFlushBeforePrinting) {
  // This test verifies that if an autoflush is triggered before printing,
  // and the canvas is not cleared in the beforeprint handler, then the canvas
  // cannot be vector printed.
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<canvas id='c' width=200 height=100></canvas>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  // Note: source_canvas is 20x20, which consumes 1600 bytes for pixel data,
  // which is larger than the 1KB limit set in PrintContextTest::SetUp().
  script_element->setTextContent(
      "source_canvas = document.createElement('canvas');"
      "source_canvas.width = 20;"
      "source_canvas.height = 20;"
      "source_ctx = source_canvas.getContext('2d');"
      "source_ctx.fillRect(0, 0, 1, 1);"
      "ctx = document.getElementById('c').getContext('2d');"
      "ctx.fillRect(0, 0, 100, 100);"
      "ctx.drawImage(source_canvas, 101, 0);"
      // Next op triggers an auto-flush due to exceeded memory limit
      "ctx.fillRect(0, 0, 1, 1);"
      "window.addEventListener('beforeprint', (ev) => {"
      "  ctx.fillRect(0, 0, 1, 1);"
      "});");
  GetDocument().body()->AppendChild(script_element);

  // Verify that the auto-flush caused the canvas printing to fall out of
  // vector mode.
  testing::Sequence s;
  // The bitmap blit
  EXPECT_CALL(canvas, onDrawImageRect2(_, _, _, _, _, _)).InSequence(s);
  // The fill rect in the event listener should leave no trace here because
  // it is supposed to be included in the canvas blit.
  EXPECT_CALL(canvas, onDrawRect(_, _))
      .Times(testing::Exactly(0))
      .InSequence(s);

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
  gfx::SizeF page_size(400, 400);
  float maximum_shrink_ratio = 1.1;
  auto* parent = GetDocument().documentElement();
  // The child document element inside iframe.
  auto* child = ChildDocument().documentElement();
  // The iframe element in the document.
  auto* target = GetDocument().getElementById(AtomicString("target"));

  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size),
                                          maximum_shrink_ratio);
  EXPECT_EQ(parent->OffsetWidth(), 440);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 440);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);

  GetDocument().GetFrame()->StartPrinting(WebPrintParams());
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(parent->OffsetWidth(), 800);
  EXPECT_EQ(child->OffsetWidth(), 800);
  EXPECT_EQ(target->OffsetWidth(), 800);

  ASSERT_TRUE(ChildDocument() != GetDocument());
  ChildDocument().GetFrame()->StartPrinting(WebPrintParams(page_size),
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

TEST_P(PrintContextTest,
       TransparentRootBackgroundWithShouldPrintBackgroundDisabled) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("");

  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(0);
  PrintSinglePage(canvas);
}

TEST_P(PrintContextTest,
       TransparentRootBackgroundWithShouldPrintBackgroundEnabled) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("");

  GetDocument().GetSettings()->SetShouldPrintBackgrounds(true);
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(0);
  PrintSinglePage(canvas);
}

TEST_P(PrintContextTest, WhiteRootBackgroundWithShouldPrintBackgroundDisabled) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<style>body { background: white; }</style>");

  GetDocument().GetSettings()->SetShouldPrintBackgrounds(false);
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(0);
  PrintSinglePage(canvas);
}

TEST_P(PrintContextTest, WhiteRootBackgroundWithShouldPrintBackgroundEnabled) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<style>body { background: white; }</style>");

  GetDocument().GetSettings()->SetShouldPrintBackgrounds(true);
  // We should paint the specified white background.
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(1);
  PrintSinglePage(canvas);
}

}  // namespace blink
