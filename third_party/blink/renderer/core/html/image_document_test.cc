// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/image_document.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// A non-animated jpeg image of size 50x50.
Vector<unsigned char> JpegImage() {
  Vector<unsigned char> jpeg;

  static const unsigned char kData[] = {
      0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
      0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
      0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x32, 0x00, 0x32, 0x03,
      0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
      0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x15, 0x01, 0x01, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03,
      0x11, 0x00, 0x3f, 0x00, 0x00, 0x94, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xd9};

  jpeg.Append(kData, sizeof(kData));
  return jpeg;
}

// An animated webp image of size 50x50.
Vector<unsigned char> AnimatedWebpImage() {
  Vector<unsigned char> animated_webp;

  static const unsigned char kData[] = {
      0x52, 0x49, 0x46, 0x46, 0x90, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
      0x56, 0x50, 0x38, 0x58, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
      0x31, 0x00, 0x00, 0x31, 0x00, 0x00, 0x41, 0x4e, 0x49, 0x4d, 0x06, 0x00,
      0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x41, 0x4e, 0x4d, 0x46,
      0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x00,
      0x00, 0x31, 0x00, 0x00, 0x64, 0x00, 0x00, 0x02, 0x56, 0x50, 0x38, 0x4c,
      0x15, 0x00, 0x00, 0x00, 0x2f, 0x31, 0x40, 0x0c, 0x00, 0x07, 0x10, 0xe5,
      0x8f, 0xfe, 0x07, 0x80, 0x84, 0xf0, 0x7f, 0xbd, 0x18, 0xd1, 0xff, 0x94,
      0x0b, 0x00, 0x41, 0x4e, 0x4d, 0x46, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x31, 0x00, 0x00, 0x64, 0x00,
      0x00, 0x00, 0x56, 0x50, 0x38, 0x4c, 0x15, 0x00, 0x00, 0x00, 0x2f, 0x31,
      0x40, 0x0c, 0x00, 0x07, 0xd0, 0xbf, 0x88, 0xfe, 0x07, 0x80, 0x84, 0xf0,
      0x7f, 0xbd, 0x18, 0xd1, 0xff, 0x94, 0x0b, 0x00};

  animated_webp.Append(kData, sizeof(kData));
  return animated_webp;
}
}  // namespace

class WindowToViewportScalingChromeClient : public EmptyChromeClient {
 public:
  WindowToViewportScalingChromeClient()
      : EmptyChromeClient(), scale_factor_(1.f) {}

  void SetScalingFactor(float s) { scale_factor_ = s; }
  float WindowToViewportScalar(LocalFrame*, const float s) const override {
    return s * scale_factor_;
  }

 private:
  float scale_factor_;
};

class ImageDocumentTest : public testing::Test {
 protected:
  void TearDown() override {
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  void CreateDocumentWithoutLoadingImage(int view_width,
                                         int view_height,
                                         bool is_animated);
  void CreateDocument(int view_width,
                      int view_height,
                      bool is_animated = false);

  ImageDocument& GetDocument() const;

  int ImageWidth() const { return GetDocument().ImageElement()->width(); }
  int ImageHeight() const { return GetDocument().ImageElement()->height(); }

  void SetPageZoom(float);
  void SetWindowToViewportScalingFactor(float);
  void SetForceZeroLayoutHeight(bool);

 private:
  test::TaskEnvironment task_environment_;
  Persistent<WindowToViewportScalingChromeClient> chrome_client_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  float zoom_factor_ = 0.0f;
  float viewport_scaling_factor_ = 0.0f;
  std::optional<bool> force_zero_layout_height_;
};

void ImageDocumentTest::CreateDocumentWithoutLoadingImage(int view_width,
                                                          int view_height,
                                                          bool is_animated) {
  chrome_client_ = MakeGarbageCollected<WindowToViewportScalingChromeClient>();
  dummy_page_holder_ = nullptr;
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(
      gfx::Size(view_width, view_height), chrome_client_);

  if (zoom_factor_) {
    dummy_page_holder_->GetFrame().SetLayoutZoomFactor(zoom_factor_);
  }
  if (viewport_scaling_factor_)
    chrome_client_->SetScalingFactor(viewport_scaling_factor_);
  if (force_zero_layout_height_.has_value()) {
    dummy_page_holder_->GetPage().GetSettings().SetForceZeroLayoutHeight(
        force_zero_layout_height_.value());
  }

  auto params = std::make_unique<WebNavigationParams>();
  params->url = is_animated ? KURL("http://www.example.com/image.webp")
                            : KURL("http://www.example.com/image.jpg");

  const Vector<unsigned char>& data =
      is_animated ? AnimatedWebpImage() : JpegImage();
  WebNavigationParams::FillStaticResponse(
      params.get(), is_animated ? "image/webp" : "image/jpeg", "UTF-8",
      base::as_chars(base::span(data)));
  dummy_page_holder_->GetFrame().Loader().CommitNavigation(std::move(params),
                                                           nullptr);
}

void ImageDocumentTest::CreateDocument(int view_width,
                                       int view_height,
                                       bool is_animated /*=false*/) {
  CreateDocumentWithoutLoadingImage(view_width, view_height, is_animated);
  blink::test::RunPendingTasks();
}

ImageDocument& ImageDocumentTest::GetDocument() const {
  Document* document = dummy_page_holder_->GetFrame().DomWindow()->document();
  ImageDocument* image_document = static_cast<ImageDocument*>(document);
  return *image_document;
}

void ImageDocumentTest::SetPageZoom(float factor) {
  zoom_factor_ = factor;
  if (dummy_page_holder_)
    dummy_page_holder_->GetFrame().SetLayoutZoomFactor(factor);
}

void ImageDocumentTest::SetWindowToViewportScalingFactor(float factor) {
  viewport_scaling_factor_ = factor;
  if (chrome_client_)
    chrome_client_->SetScalingFactor(factor);
}

void ImageDocumentTest::SetForceZeroLayoutHeight(bool force) {
  force_zero_layout_height_ = force;
  if (dummy_page_holder_) {
    dummy_page_holder_->GetPage().GetSettings().SetForceZeroLayoutHeight(force);
  }
}

TEST_F(ImageDocumentTest, ImageLoad) {
  CreateDocument(50, 50);
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, LargeImageScalesDown) {
  CreateDocument(25, 30);
  EXPECT_EQ(25, ImageWidth());
  EXPECT_EQ(25, ImageHeight());

  CreateDocument(35, 20);
  EXPECT_EQ(20, ImageWidth());
  EXPECT_EQ(20, ImageHeight());
}

TEST_F(ImageDocumentTest, RestoreImageOnClick) {
  CreateDocument(30, 40);
  GetDocument().ImageClicked(4, 4);
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, InitialZoomDoesNotAffectScreenFit) {
  SetPageZoom(2.f);
  CreateDocument(20, 10);
  EXPECT_EQ(10, ImageWidth());
  EXPECT_EQ(10, ImageHeight());
  GetDocument().ImageClicked(4, 4);
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, ZoomingDoesNotChangeRelativeSize) {
  CreateDocument(75, 75);
  SetPageZoom(0.5f);
  GetDocument().WindowSizeChanged();
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
  SetPageZoom(2.f);
  GetDocument().WindowSizeChanged();
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, ImageScalesDownWithDsf) {
  SetWindowToViewportScalingFactor(2.f);
  CreateDocument(20, 30);
  EXPECT_EQ(10, ImageWidth());
  EXPECT_EQ(10, ImageHeight());
}

TEST_F(ImageDocumentTest, ImageNotCenteredWithForceZeroLayoutHeight) {
  SetForceZeroLayoutHeight(true);
  CreateDocument(80, 70);
  EXPECT_FALSE(GetDocument().ShouldShrinkToFit());
  EXPECT_EQ(0, GetDocument().ImageElement()->OffsetLeft());
  EXPECT_EQ(0, GetDocument().ImageElement()->OffsetTop());
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, ImageCenteredWithoutForceZeroLayoutHeight) {
  SetForceZeroLayoutHeight(false);
  CreateDocument(80, 70);
  EXPECT_TRUE(GetDocument().ShouldShrinkToFit());
  EXPECT_EQ(15, GetDocument().ImageElement()->OffsetLeft());
  EXPECT_EQ(10, GetDocument().ImageElement()->OffsetTop());
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, DomInteractive) {
  CreateDocument(25, 30);
  EXPECT_FALSE(GetDocument().GetTiming().DomInteractive().is_null());
}

TEST_F(ImageDocumentTest, ImageSrcChangedBeforeFinish) {
  CreateDocumentWithoutLoadingImage(80, 70, /*is_animated*/ false);
  GetDocument().ImageElement()->removeAttribute(html_names::kSrcAttr);
  blink::test::RunPendingTasks();
}

TEST_F(ImageDocumentTest, ImageStyleContainsTransitionForNonAnimatedImage) {
  CreateDocument(50, 50);
  auto& style =
      GetDocument().ImageElement()->getAttribute(html_names::kStyleAttr);
  EXPECT_NE(style.Find("transition:"), kNotFound);
}

TEST_F(ImageDocumentTest, ImageStyleDoesNotContainTransitionForAnimatedImage) {
  CreateDocument(50, 50, /*is_animated*/ true);
  auto& style =
      GetDocument().ImageElement()->getAttribute(html_names::kStyleAttr);
  EXPECT_EQ(style.Find("transition:"), kNotFound);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE(test) DISABLED_##test
#else
#define MAYBE(test) test
#endif

TEST_F(ImageDocumentTest, MAYBE(ImageCenteredAtDeviceScaleFactor)) {
  SetWindowToViewportScalingFactor(1.5f);
  CreateDocument(30, 30);

  EXPECT_TRUE(GetDocument().ShouldShrinkToFit());
  GetDocument().ImageClicked(15, 27);
  ScrollOffset offset =
      GetDocument().GetFrame()->View()->LayoutViewport()->GetScrollOffset();
  EXPECT_EQ(20, offset.x());
  EXPECT_EQ(20, offset.y());

  GetDocument().ImageClicked(20, 20);

  GetDocument().ImageClicked(12, 15);
  offset =
      GetDocument().GetFrame()->View()->LayoutViewport()->GetScrollOffset();
  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(11.25f, offset.x());
    EXPECT_EQ(20, offset.y());
  } else {
    EXPECT_EQ(11, offset.x());
    EXPECT_EQ(20, offset.y());
  }
}

class ImageDocumentViewportTest : public SimTest {
 public:
  ImageDocumentViewportTest() = default;
  ~ImageDocumentViewportTest() override = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetViewportMetaEnabled(true);
    WebView().GetSettings()->SetShrinksViewportContentToFit(true);
    WebView().GetSettings()->SetMainFrameResizesAreOrientationChanges(true);
  }

  VisualViewport& GetVisualViewport() {
    return WebView().GetPage()->GetVisualViewport();
  }

  ImageDocument& GetDocument() {
    Document* document = To<LocalFrame>(WebView().GetPage()->MainFrame())
                             ->DomWindow()
                             ->document();
    ImageDocument* image_document = static_cast<ImageDocument*>(document);
    return *image_document;
  }
};

// Tests that hiding the URL bar doesn't cause a "jump" when viewing an image
// much wider than the viewport.
TEST_F(ImageDocumentViewportTest, HidingURLBarDoesntChangeImageLocation) {
  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());

  // Initialize with the URL bar showing. Make the viewport very thin so that
  // we load an image much wider than the viewport but fits vertically. The
  // page will load zoomed out so the image will be vertically centered.
  WebView().ResizeWithBrowserControls(gfx::Size(5, 40), 10, 10, true);
  SimRequest request("https://example.com/test.jpg", "image/jpeg");
  LoadURL("https://example.com/test.jpg");

  Vector<char> data;
  data.AppendVector(JpegImage());
  request.Complete(data);

  Compositor().BeginFrame();

  HTMLImageElement* img = GetDocument().ImageElement();
  DOMRect* rect = img->GetBoundingClientRect();

  // Some initial sanity checking. We'll use the BoundingClientRect for the
  // image location since that's relative to the layout viewport and the layout
  // viewport is unscrollable in this test. Since the image is 10X wider than
  // the viewport, we'll zoom out to 0.1. This means the layout viewport is 400
  // pixels high so the image will be centered in that.
  ASSERT_EQ(50u, img->width());
  ASSERT_EQ(50u, img->height());
  ASSERT_EQ(0.1f, GetVisualViewport().Scale());
  ASSERT_EQ(0, rect->x());
  ASSERT_EQ(175, rect->y());

  // Hide the URL bar. This will make the viewport taller but won't change the
  // layout size so the image location shouldn't change.
  WebView().ResizeWithBrowserControls(gfx::Size(5, 50), 10, 10, false);
  Compositor().BeginFrame();
  rect = img->GetBoundingClientRect();
  EXPECT_EQ(50, rect->width());
  EXPECT_EQ(50, rect->height());
  EXPECT_EQ(0, rect->x());
  EXPECT_EQ(125, rect->y());
}

TEST_F(ImageDocumentViewportTest, ScaleImage) {
  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  SimRequest request("https://example.com/test.jpg", "image/jpeg");
  LoadURL("https://example.com/test.jpg");

  Vector<char> data;
  data.AppendVector(JpegImage());
  request.Complete(data);

  HTMLImageElement* img = GetDocument().ImageElement();

  // no zoom
  WebView().MainFrameWidget()->Resize(gfx::Size(100, 100));
  WebView().SetZoomFactorForDeviceScaleFactor(1.f);
  Compositor().BeginFrame();
  EXPECT_EQ(50u, img->width());
  EXPECT_EQ(50u, img->height());
  EXPECT_EQ(100, GetDocument().CalculateDivWidth());
  EXPECT_EQ(1.f, GetVisualViewport().Scale());
  EXPECT_EQ(100, GetVisualViewport().Width());
  EXPECT_EQ(100, GetVisualViewport().Height());

  // zoom-for-dsf = 4. WebView size is in physical pixel(400*400), image and
  // visual viewport should be same in CSS pixel, as no dsf applied.
  // This simulates running on two phones with different screen densities but
  // same (physical) screen size, image document should displayed the same.
  WebView().MainFrameWidget()->Resize(gfx::Size(400, 400));
  WebView().SetZoomFactorForDeviceScaleFactor(4.f);
  Compositor().BeginFrame();
  EXPECT_EQ(50u, img->width());
  EXPECT_EQ(50u, img->height());
  EXPECT_EQ(100, GetDocument().CalculateDivWidth());
  EXPECT_EQ(1.f, GetVisualViewport().Scale());
  EXPECT_EQ(100, GetVisualViewport().Width());
  EXPECT_EQ(100, GetVisualViewport().Height());
}

// Tests that with zoom factor for device scale factor, image with different
// size fit in the viewport correctly.
TEST_F(ImageDocumentViewportTest, DivWidth) {
  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  SimRequest request("https://example.com/test.jpg", "image/jpeg");
  LoadURL("https://example.com/test.jpg");

  Vector<char> data;
  data.AppendVector(JpegImage());
  request.Complete(data);

  HTMLImageElement* img = GetDocument().ImageElement();

  WebView().SetZoomFactorForDeviceScaleFactor(2.f);

  // Image smaller then webview size, visual viewport is not zoomed, and image
  // will be centered in the viewport.
  WebView().MainFrameWidget()->Resize(gfx::Size(200, 200));
  Compositor().BeginFrame();
  EXPECT_EQ(50u, img->width());
  EXPECT_EQ(50u, img->height());
  EXPECT_EQ(100, GetDocument().CalculateDivWidth());
  EXPECT_EQ(1.f, GetVisualViewport().Scale());
  EXPECT_EQ(100, GetVisualViewport().Width());
  EXPECT_EQ(100, GetVisualViewport().Height());
  DOMRect* rect = img->GetBoundingClientRect();
  EXPECT_EQ(25, rect->x());
  EXPECT_EQ(25, rect->y());

  // Image wider than webview size, image should fill the visual viewport, and
  // visual viewport zoom out to 0.5.
  WebView().MainFrameWidget()->Resize(gfx::Size(50, 50));
  Compositor().BeginFrame();
  EXPECT_EQ(50u, img->width());
  EXPECT_EQ(50u, img->height());
  EXPECT_EQ(50, GetDocument().CalculateDivWidth());
  EXPECT_EQ(0.5f, GetVisualViewport().Scale());
  EXPECT_EQ(50, GetVisualViewport().Width());
  EXPECT_EQ(50, GetVisualViewport().Height());

  // When image is more than 10X wider than webview, shrink the image to fit the
  // width of the screen.
  WebView().MainFrameWidget()->Resize(gfx::Size(4, 20));
  Compositor().BeginFrame();
  EXPECT_EQ(20u, img->width());
  EXPECT_EQ(20u, img->height());
  EXPECT_EQ(20, GetDocument().CalculateDivWidth());
  EXPECT_EQ(0.1f, GetVisualViewport().Scale());
  EXPECT_EQ(20, GetVisualViewport().Width());
  EXPECT_EQ(100, GetVisualViewport().Height());
  rect = img->GetBoundingClientRect();
  EXPECT_EQ(0, rect->x());
  EXPECT_EQ(40, rect->y());
}

#undef MAYBE
}  // namespace blink
