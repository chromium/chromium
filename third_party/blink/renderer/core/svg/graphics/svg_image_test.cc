// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class SVGImageTest : public testing::Test, private ScopedMockOverlayScrollbars {
 public:
  SVGImage& GetImage() { return *image_; }

  void Load(const char* data, bool should_pause) {
    observer_ = MakeGarbageCollected<PauseControlImageObserver>(should_pause);
    image_ = SVGImage::Create(observer_);
    image_->SetData(SharedBuffer::Create(data, strlen(data)), true);
    test::RunPendingTasks();
  }

  void LoadUsingFileName(const String& file_name) {
    String file_path = test::BlinkWebTestsDir() + file_name;
    std::optional<Vector<char>> data = test::ReadFromFile(file_path);
    EXPECT_TRUE(data && data->size());
    scoped_refptr<SharedBuffer> image_data =
        SharedBuffer::Create(std::move(*data));

    observer_ = MakeGarbageCollected<PauseControlImageObserver>(true);
    image_ = SVGImage::Create(observer_);
    image_->SetData(image_data, true);
    test::RunPendingTasks();
  }

  void PumpFrame() {
    Image* image = image_.get();
    std::unique_ptr<SkCanvas> null_canvas = SkMakeNullCanvas();
    SkiaPaintCanvas canvas(null_canvas.get());
    cc::PaintFlags flags;
    gfx::RectF dummy_rect(0, 0, 100, 100);
    image->Draw(&canvas, flags, dummy_rect, dummy_rect, ImageDrawOptions());
  }

 private:
  class PauseControlImageObserver
      : public GarbageCollected<PauseControlImageObserver>,
        public ImageObserver {
   public:
    PauseControlImageObserver(bool should_pause)
        : should_pause_(should_pause) {}

    void DecodedSizeChangedTo(const Image*, size_t new_size) override {}

    bool ShouldPauseAnimation(const Image*) override { return should_pause_; }

    void Changed(const Image*) override {}

    void AsyncLoadCompleted(const blink::Image*) override {}

    void Trace(Visitor* visitor) const override {
      ImageObserver::Trace(visitor);
    }

   private:
    bool should_pause_;
  };
  test::TaskEnvironment task_environment_;
  Persistent<PauseControlImageObserver> observer_;
  scoped_refptr<SVGImage> image_;
};

const char kAnimatedDocument[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
    "<style>"
    "@keyframes rot {"
    " from { transform: rotate(0deg); } to { transform: rotate(-360deg); }"
    "}"
    ".spinner {"
    " transform-origin: 50%% 50%%;"
    " animation-name: rot;"
    " animation-duration: 4s;"
    " animation-iteration-count: infinite;"
    " animation-timing-function: linear;"
    "}"
    "</style>"
    "<path class='spinner' fill='none' d='M 8,1.125 A 6.875,6.875 0 1 1 "
    "1.125,8' stroke-width='2' stroke='blue'/>"
    "</svg>";

TEST_F(SVGImageTest, TimelineSuspendAndResume) {
  const bool kShouldPause = true;
  Load(kAnimatedDocument, kShouldPause);
  SVGImageChromeClient& chrome_client = GetImage().ChromeClientForTesting();
  DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>* timer =
      MakeGarbageCollected<
          DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>>(
          scheduler::GetSingleThreadTaskRunnerForTesting(), &chrome_client,
          &SVGImageChromeClient::AnimationTimerFired);
  chrome_client.SetTimerForTesting(timer);

  // Simulate a draw. Cause a frame (timer) to be scheduled.
  PumpFrame();
  EXPECT_TRUE(GetImage().MaybeAnimated());
  EXPECT_TRUE(timer->Value().IsActive());

  // Fire the timer/trigger a frame update. Since the observer always returns
  // true for shouldPauseAnimation, this will result in the timeline being
  // suspended.
  test::RunDelayedTasks(base::Milliseconds(1) +
                        timer->Value().NextFireInterval());
  EXPECT_TRUE(chrome_client.IsSuspended());
  EXPECT_FALSE(timer->Value().IsActive());

  // Simulate a draw. This should resume the animation again.
  PumpFrame();
  EXPECT_TRUE(timer->Value().IsActive());
  EXPECT_FALSE(chrome_client.IsSuspended());
}

TEST_F(SVGImageTest, ResetAnimation) {
  const bool kShouldPause = false;
  Load(kAnimatedDocument, kShouldPause);
  SVGImageChromeClient& chrome_client = GetImage().ChromeClientForTesting();
  DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>* timer =
      MakeGarbageCollected<
          DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>>(
          scheduler::GetSingleThreadTaskRunnerForTesting(), &chrome_client,
          &SVGImageChromeClient::AnimationTimerFired);
  chrome_client.SetTimerForTesting(timer);

  // Simulate a draw. Cause a frame (timer) to be scheduled.
  PumpFrame();
  EXPECT_TRUE(GetImage().MaybeAnimated());
  EXPECT_TRUE(timer->Value().IsActive());

  // Reset the animation. This will suspend the timeline but not cancel the
  // timer.
  GetImage().ResetAnimation();
  EXPECT_TRUE(chrome_client.IsSuspended());
  EXPECT_TRUE(timer->Value().IsActive());

  // Fire the timer/trigger a frame update. The timeline will remain
  // suspended and no frame will be scheduled.
  test::RunDelayedTasks(base::Milliseconds(1) +
                        timer->Value().NextFireInterval());
  EXPECT_TRUE(chrome_client.IsSuspended());
  EXPECT_FALSE(timer->Value().IsActive());

  // Simulate a draw. This should resume the animation again.
  PumpFrame();
  EXPECT_FALSE(chrome_client.IsSuspended());
  EXPECT_TRUE(timer->Value().IsActive());
}

TEST_F(SVGImageTest, SupportsSubsequenceCaching) {
  const bool kShouldPause = true;
  Load(kAnimatedDocument, kShouldPause);
  PumpFrame();
  LocalFrame* local_frame =
      To<LocalFrame>(GetImage().GetPageForTesting()->MainFrame());
  EXPECT_TRUE(local_frame->GetDocument()->IsSVGDocument());
  LayoutObject* svg_root = local_frame->View()->GetLayoutView()->FirstChild();
  EXPECT_TRUE(svg_root->IsSVGRoot());
  EXPECT_TRUE(To<LayoutBoxModelObject>(svg_root)
                  ->Layer()
                  ->SupportsSubsequenceCaching());
}

TEST_F(SVGImageTest, LayoutShiftTrackerDisabled) {
  const bool kDontPause = false;
  Load("<svg xmlns='http://www.w3.org/2000/svg'></svg>", kDontPause);
  LocalFrame* local_frame =
      To<LocalFrame>(GetImage().GetPageForTesting()->MainFrame());
  EXPECT_TRUE(local_frame->GetDocument()->IsSVGDocument());
  auto& layout_shift_tracker = local_frame->View()->GetLayoutShiftTracker();
  EXPECT_FALSE(layout_shift_tracker.IsActive());
}

TEST_F(SVGImageTest, SetSizeOnVisualViewport) {
  const bool kDontPause = false;
  Load(
      "<svg xmlns='http://www.w3.org/2000/svg'>"
      "   <rect id='green' width='100%' height='100%' fill='green' />"
      "</svg>",
      kDontPause);
  PumpFrame();
  LocalFrame* local_frame =
      To<LocalFrame>(GetImage().GetPageForTesting()->MainFrame());
  ASSERT_FALSE(local_frame->View()->Size().IsEmpty());
  EXPECT_EQ(local_frame->View()->Size(),
            GetImage().GetPageForTesting()->GetVisualViewport().Size());
}

TEST_F(SVGImageTest, IsSizeAvailable) {
  const bool kShouldPause = false;
  Load("<svg xmlns='http://www.w3.org/2000/svg'></svg>", kShouldPause);
  EXPECT_TRUE(GetImage().IsSizeAvailable());

  Load("<notsvg></notsvg>", kShouldPause);
  EXPECT_FALSE(GetImage().IsSizeAvailable());

  Load("<notsvg xmlns='http://www.w3.org/2000/svg'></notsvg>", kShouldPause);
  EXPECT_FALSE(GetImage().IsSizeAvailable());
}

TEST_F(SVGImageTest, DisablesSMILEvents) {
  const bool kShouldPause = true;
  Load(kAnimatedDocument, kShouldPause);
  LocalFrame* local_frame =
      To<LocalFrame>(GetImage().GetPageForTesting()->MainFrame());
  EXPECT_TRUE(local_frame->GetDocument()->IsSVGDocument());
  SMILTimeContainer* time_container =
      To<SVGSVGElement>(local_frame->GetDocument()->documentElement())
          ->TimeContainer();
  EXPECT_TRUE(time_container->EventsDisabled());
}

TEST_F(SVGImageTest, PaintFrameForCurrentFrameWithMQAndZoom) {
  const bool kShouldPause = false;
  Load(R"SVG(
         <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'>
           <style>@media(max-width:50px){rect{fill:blue}}</style>
           <rect width='10' height='10' fill='red'/>
         </svg>)SVG",
       kShouldPause);

  auto container =
      SVGImageForContainer::Create(GetImage(), gfx::SizeF(100, 100), 2, nullptr,
                                   mojom::blink::PreferredColorScheme::kLight);
  SkBitmap bitmap =
      container->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  ASSERT_EQ(bitmap.width(), 100);
  ASSERT_EQ(bitmap.height(), 100);
  EXPECT_EQ(bitmap.getColor(10, 10), SK_ColorBLUE);
  EXPECT_EQ(bitmap.getColor(90, 10), SK_ColorBLUE);
  EXPECT_EQ(bitmap.getColor(10, 90), SK_ColorBLUE);
  EXPECT_EQ(bitmap.getColor(90, 90), SK_ColorBLUE);
}

TEST_F(SVGImageTest, SVGWithSmilAnimationIsAnimated) {
  const bool kShouldPause = true;
  Load(R"SVG(
         <svg xmlns="http://www.w3.org/2000/svg">
           <rect width="10" height="10"/>
           <animateTransform attributeName="transform" type="rotate"
                             from="0 5 5" to="360 5 5" dur="1s"
                             repeatCount="indefinite"/>
         </svg>)SVG",
       kShouldPause);

  EXPECT_TRUE(GetImage().MaybeAnimated());
}

TEST_F(SVGImageTest, NestedSVGWithSmilAnimationIsAnimated) {
  const bool kShouldPause = true;
  Load(R"SVG(
         <svg xmlns="http://www.w3.org/2000/svg">
           <svg>
             <rect width="10" height="10"/>
             <animateTransform attributeName="transform" type="rotate"
                               from="0 5 5" to="360 5 5" dur="1s"
                               repeatCount="indefinite"/>
           </svg>
         </svg>)SVG",
       kShouldPause);

  EXPECT_TRUE(GetImage().MaybeAnimated());
}

class SVGImageSimTest : public SimTest, private ScopedMockOverlayScrollbars {
 public:
  static void WaitForTimer(TimerBase& timer) {
    if (!timer.IsActive()) {
      return;
    }
    test::RunDelayedTasks(base::Milliseconds(1) + timer.NextFireInterval());
  }
};

TEST_F(SVGImageSimTest, PageVisibilityHiddenToVisible) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.svg",
                                       "image/svg+xml");
  LoadURL("https://example.com/");
  main_resource.Complete("<img src='image.svg' width='100' id='image'>");
  image_resource.Complete(kAnimatedDocument);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  Element* element = GetDocument().getElementById(AtomicString("image"));
  ASSERT_TRUE(IsA<HTMLImageElement>(element));

  ImageResourceContent* image_content =
      To<HTMLImageElement>(*element).CachedImage();
  ASSERT_TRUE(image_content);
  ASSERT_TRUE(image_content->IsLoaded());
  ASSERT_TRUE(image_content->HasImage());
  Image* image = image_content->GetImage();
  ASSERT_TRUE(IsA<SVGImage>(image));
  SVGImageChromeClient& svg_image_chrome_client =
      To<SVGImage>(*image).ChromeClientForTesting();
  TimerBase& timer = svg_image_chrome_client.GetTimerForTesting();

  // Wait for the next animation frame to be triggered, and then trigger a new
  // frame. The image animation timeline should be running.
  WaitForTimer(timer);
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());

  // Set page visibility to 'hidden', and then wait for the animation timer to
  // fire. This should suspend the image animation. (Suspend the image's
  // animation timeline.)
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*initial_state=*/false);
  test::RunDelayedTasks(base::Milliseconds(1) + timer.NextFireInterval());

  EXPECT_TRUE(svg_image_chrome_client.IsSuspended());

  // Set page visibility to 'visible' - this should schedule a new animation
  // frame and resume the image animation.
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*initial_state=*/false);
  test::RunDelayedTasks(base::Milliseconds(1) + timer.NextFireInterval());
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());
}

const char kSmilAnimatedDocument[] = R"SVG(
<svg xmlns='http://www.w3.org/2000/svg' fill='red' width='10' height='10'>
  <circle cx='5' cy='5'>
    <animate attributeName='r' values='0; 10; 0' dur='10s'
             repeatCount='indefinite'/>
  </circle>
</svg>
)SVG";

TEST_F(SVGImageSimTest, AnimationsPausedWhenImageScrolledOutOfView) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.svg",
                                       "image/svg+xml");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <img src="image.svg" width="20" id="image">
    <div style="height: 10000px"></div>
  )HTML");
  image_resource.Complete(kSmilAnimatedDocument);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  Element* element = GetDocument().getElementById(AtomicString("image"));
  ASSERT_TRUE(IsA<HTMLImageElement>(element));

  ImageResourceContent* image_content =
      To<HTMLImageElement>(*element).CachedImage();
  ASSERT_TRUE(image_content);
  ASSERT_TRUE(image_content->IsLoaded());
  ASSERT_TRUE(image_content->HasImage());
  Image* image = image_content->GetImage();
  ASSERT_TRUE(IsA<SVGImage>(image));
  SVGImage& svg_image = To<SVGImage>(*image);
  ASSERT_TRUE(svg_image.MaybeAnimated());
  auto& svg_image_chrome_client = svg_image.ChromeClientForTesting();
  TimerBase& timer = svg_image_chrome_client.GetTimerForTesting();

  // Wait for the next animation frame to be triggered, and then trigger a new
  // frame. The image animation timeline should be running.
  WaitForTimer(timer);
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());
  EXPECT_TRUE(timer.IsActive());

  // Scroll down to the bottom of the document to move the image out of the
  // viewport, and then wait for the animation timer to fire. This triggers an
  // "image changed" notification, which (re)sets the delay-invalidation
  // flag. The following begin-frame then observes that the image is not
  // visible.
  GetDocument().domWindow()->scrollBy(0, 10000);
  test::RunDelayedTasks(base::Milliseconds(1) + timer.NextFireInterval());
  Compositor().BeginFrame();
  EXPECT_TRUE(timer.IsActive());

  // Trigger another animation frame. This makes the WillRenderImage() query
  // return false (because delay-invalidation is set), which in turn suspends
  // the image animation. (Suspend the image's animation timeline.)
  test::RunDelayedTasks(base::Milliseconds(1) + timer.NextFireInterval());

  EXPECT_TRUE(svg_image_chrome_client.IsSuspended());
  EXPECT_FALSE(timer.IsActive());

  // Scroll back up to make the image visible. The following paint observes
  // that the image is now visible, and triggers a paint that resume the image
  // animation.
  GetDocument().domWindow()->scrollBy(0, -10000);
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());
  EXPECT_TRUE(timer.IsActive());
}

TEST_F(SVGImageSimTest, AnimationsResumedWhenImageScrolledIntoView) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.svg",
                                       "image/svg+xml");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      .change {
        will-change: transform;
      }
    </style>
    <div style="height: 100vh"></div>
    <div class="change">
      <img src="image.svg" width="20" id="image">
    </div>
  )HTML");
  image_resource.Complete(kAnimatedDocument);

  Compositor().BeginFrame();

  Element* element = GetDocument().getElementById(AtomicString("image"));
  ASSERT_TRUE(IsA<HTMLImageElement>(element));

  ImageResourceContent* image_content =
      To<HTMLImageElement>(*element).CachedImage();
  ASSERT_TRUE(image_content);
  ASSERT_TRUE(image_content->IsLoaded());
  ASSERT_TRUE(image_content->HasImage());
  Image* image = image_content->GetImage();
  ASSERT_TRUE(IsA<SVGImage>(image));
  SVGImage& svg_image = To<SVGImage>(*image);
  ASSERT_TRUE(svg_image.MaybeAnimated());
  auto& svg_image_chrome_client = svg_image.ChromeClientForTesting();
  TimerBase& timer = svg_image_chrome_client.GetTimerForTesting();

  // The image animation is running after being started by the paint above.
  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());
  EXPECT_TRUE(timer.IsActive());

  // Process pending timers. This will suspend the image animation.
  WaitForTimer(timer);
  WaitForTimer(timer);

  EXPECT_TRUE(svg_image_chrome_client.IsSuspended());
  EXPECT_FALSE(timer.IsActive());

  // Mutate the image's container triggering a paint that restarts the image
  // animation.
  Element* div = element->parentElement();
  div->removeAttribute(html_names::kClassAttr);

  Compositor().BeginFrame();

  // Wait for the next animation frame.
  WaitForTimer(timer);

  // Scroll down to make the image appear in the viewport, and then wait for
  // the animation timer to fire.
  GetDocument().domWindow()->scrollBy(0, 10000);
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());
  EXPECT_TRUE(timer.IsActive());
}

TEST_F(SVGImageSimTest, TwoImagesSameSVGImageDifferentSize) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.svg",
                                       "image/svg+xml");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <img src="image.svg" style="width: 100px">
    <img src="image.svg" style="width: 200px">
  )HTML");
  image_resource.Complete(R"SVG(
    <svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
       <rect fill="green" width="100" height="100"/>
    </svg>
  )SVG");

  Compositor().BeginFrame();
  test::RunPendingTasks();
  // The previous frame should result in a stable state and should not schedule
  // new visual updates.
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
}

TEST_F(SVGImageSimTest, SVGWithXSLT) {
  // To make "https" scheme counted as "Blink.UseCounter.Extensions.Features",
  // we should make it recognized as an extension.
  CommonSchemeRegistry::RegisterURLSchemeAsExtension("https");

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.svg",
                                       "image/svg+xml");

  base::HistogramTester histograms;
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <img src="image.svg">
  )HTML");
  image_resource.Complete(R"SVG(<?xml version="1.0"?>
<?xml-stylesheet type="text/xsl" href="#stylesheet"?>
<!DOCTYPE svg [
<!ATTLIST xsl:stylesheet
id ID #REQUIRED>
]>
<svg>
    <xsl:stylesheet id="stylesheet" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:svg="http://www.w3.org/2000/svg"></xsl:stylesheet>
</svg>)SVG");

  Compositor().BeginFrame();
  test::RunPendingTasks();
  // The previous frame should result in a stable state and should not schedule
  // new visual updates.
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  // Ensure |UseCounter.DidCommitLoad| is called once.
  // Since we cannot use |UseCounter.IsCounted(WebFeature::kPageVisits)|, we
  // check the histogram updated in |DidCommitLoad|.
  histograms.ExpectBucketCount("Blink.UseCounter.Extensions.Features",
                               WebFeature::kPageVisits, 1);
}

namespace {

size_t CountPaintOpType(const cc::PaintRecord& record, cc::PaintOpType type) {
  size_t count = 0;
  for (const cc::PaintOp& op : record) {
    if (op.IsPaintOpWithFlags()) {
      const cc::PaintFlags& flags =
          static_cast<const cc::PaintOpWithFlags&>(op).flags;
      if (const cc::PaintShader* shader = flags.getShader()) {
        if (shader->shader_type() == cc::PaintShader::Type::kPaintRecord) {
          count += CountPaintOpType(*shader->paint_record(), type);
        }
      }
    }
    if (op.GetType() == type) {
      ++count;
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      count += CountPaintOpType(record_op.record, type);
    }
  }
  return count;
}

}  // namespace

// Tests the culling of invisible sprites from a larger sprite sheet.
TEST_F(SVGImageSimTest, SpriteSheetCulling) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<style>"
      "  body { zoom: 2.5; }"
      "  #div {"
      "    width: 100px;"
      "    height: 100px;"
      "    background-image: url(\"data:image/svg+xml,"
      "      <svg xmlns='http://www.w3.org/2000/svg' width='100' height='300'>"
      "        <circle cx='50' cy='50' r='10' fill='red'/>"
      "        <circle cx='50' cy='150' r='10' fill='green'/>"
      "        <circle cx='25' cy='250' r='10' fill='blue'/>"
      "        <circle cx='50' cy='250' r='10' fill='blue'/>"
      "        <circle cx='75' cy='250' r='10' fill='blue'/>"
      "      </svg>\");"
      "    background-position-y: -100px;"
      "    background-repeat: no-repeat;"
      "  }"
      "</style>"
      "<div id='div'></div>");

  Compositor().BeginFrame();

  // Initially, only the green circle should be recorded.
  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the height so one green circle and three blue circles are visible,
  // and ensure four circles are recorded.
  Element* div = GetDocument().getElementById(AtomicString("div"));
  div->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(4U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the background position so only the three blue circles are visible,
  // and ensure three circles are recorded.
  div->setAttribute(
      html_names::kStyleAttr,
      AtomicString("height: 200px; background-position-y: -200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(3U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));
}

// Tests the culling of invisible sprites from a larger sprite sheet where the
// element also has a border-radius. This is intended to cover the
// Image::ApplyShader() fast-path in GraphicsContext::DrawImageRRect().
TEST_F(SVGImageSimTest, SpriteSheetCullingBorderRadius) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<style>"
      "  body { zoom: 2.5; }"
      "  #div {"
      "    width: 100px;"
      "    height: 100px;"
      "    background-image: url(\"data:image/svg+xml,"
      "      <svg xmlns='http://www.w3.org/2000/svg' width='100' height='300'>"
      "        <circle cx='50' cy='50' r='10' fill='red'/>"
      "        <circle cx='50' cy='150' r='10' fill='green'/>"
      "        <circle cx='25' cy='250' r='10' fill='blue'/>"
      "        <circle cx='50' cy='250' r='10' fill='blue'/>"
      "        <circle cx='75' cy='250' r='10' fill='blue'/>"
      "      </svg>\");"
      "    background-position-y: -100px;"
      "    background-repeat: no-repeat;"
      "    border-radius: 5px;"
      "  }"
      "</style>"
      "<div id='div'></div>");

  Compositor().BeginFrame();

  // Initially, only the green circle should be recorded.
  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawRRect));
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the height so one green circle and three blue circles are visible,
  // and ensure four circles are recorded.
  Element* div = GetDocument().getElementById(AtomicString("div"));
  div->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawRRect));
  EXPECT_EQ(4U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));
}

// Similar to `SpriteSheetCulling` but using a full-sized sprite sheet <img>
// element with absolute positioning under overflow: hidden. This pattern is
// used by Google Docs.
TEST_F(SVGImageSimTest, ClippedAbsoluteImageSpriteSheetCulling) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
        body { zoom: 2.5; }
        #div {
          width: 100px;
          height: 100px;
          overflow: hidden;
          position: relative;
        }
        #image {
          position: absolute;
          left: 0;
          top: -100px;
        }
      </style>
      <div id="div">
        <img id="image" src="data:image/svg+xml,
            <svg xmlns='http://www.w3.org/2000/svg' width='100' height='400'>
              <circle cx='50' cy='50' r='10' fill='red'/>
              <circle cx='50' cy='150' r='10' fill='green'/>
              <circle cx='25' cy='250' r='10' fill='blue'/>
              <circle cx='50' cy='250' r='10' fill='blue'/>
              <circle cx='75' cy='250' r='10' fill='blue'/>
            </svg>">
      </div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, only the green circle should be recorded.
  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the div's height so one green circle and three blue circles are
  // visible, and ensure four circles are recorded.
  Element* div_element = GetDocument().getElementById(AtomicString("div"));
  div_element->setAttribute(html_names::kStyleAttr,
                            AtomicString("height: 200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(4U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the image's position so only the three blue circles are visible,
  // and ensure three circles are recorded.
  Element* image_element = GetDocument().getElementById(AtomicString("image"));
  image_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("top: -200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(3U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));
}

// Similar to `SpriteSheetCulling` but using a full-sized sprite sheet <img>
// element under overflow: hidden. This differs from
// `ClippedAbsoluteImageSpriteSheetCulling` because static positioning and
// margin are used to position the image, rather than absolute positioning.
TEST_F(SVGImageSimTest, ClippedStaticImageSpriteSheetCulling) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
        body { zoom: 2.5; }
        #div {
          width: 100px;
          height: 100px;
          overflow: hidden;
          position: relative;
        }
        #image {
          margin-top: -100px;
        }
      </style>
      <div id="div">
        <img id="image" src="data:image/svg+xml,
            <svg xmlns='http://www.w3.org/2000/svg' width='100' height='400'>
              <circle cx='50' cy='50' r='10' fill='red'/>
              <circle cx='50' cy='150' r='10' fill='green'/>
              <circle cx='25' cy='250' r='10' fill='blue'/>
              <circle cx='50' cy='250' r='10' fill='blue'/>
              <circle cx='75' cy='250' r='10' fill='blue'/>
            </svg>">
      </div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, only the green circle should be recorded.
  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the div's height so one green circle and three blue circles are
  // visible, and ensure four circles are recorded.
  Element* div_element = GetDocument().getElementById(AtomicString("div"));
  div_element->setAttribute(html_names::kStyleAttr,
                            AtomicString("height: 200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(4U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the image's position so only the three blue circles are visible,
  // and ensure three circles are recorded.
  Element* image_element = GetDocument().getElementById(AtomicString("image"));
  image_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("margin-top: -200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(3U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the div's position to be fractional and ensure only three blue
  // circles are still recorded.
  div_element->setAttribute(html_names::kStyleAttr,
                            AtomicString("margin-left: 0.5px; height: 200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(3U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));
}

// Similar to `SpriteSheetCulling` but using a regular scrolling interest rect
// that isn't clipping to a specific sprite within the image. To avoid
// regressing non-sprite-sheet paint performance with additional invalidatoins,
// we want to avoid special culling in these cases.
TEST_F(SVGImageSimTest, InterestRectDoesNotCullImageSpriteSheet) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
        body { zoom: 2.5; }
        #div {
          width: 200px;
          height: 10000px;
          overflow: hidden;
          position: relative;
        }
        #image {
          position: absolute;
          left: 0;
          top: 0;
        }
      </style>
      <div id="div">
        <img id="image" src="data:image/svg+xml,
            <svg xmlns='http://www.w3.org/2000/svg' width='100' height='6000'>
              <circle cx='50' cy='50' r='10' fill='green'/>
              <circle cx='25' cy='5950' r='10' fill='blue'/>
              <circle cx='50' cy='5950' r='10' fill='blue'/>
              <circle cx='75' cy='5950' r='10' fill='blue'/>
            </svg>">
      </div>
  )HTML");

  Compositor().BeginFrame();

  // The sprite sheet optimization in `ImagePainter::PaintReplaced` should not
  // apply because the scrolling interest rect is not for a specific sprite
  // within the image, and all circles should be recorded.
  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(4U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the div's width and height so that it creates a cull rect that clips
  // to just a single circle, and ensure just one circle is recorded.
  Element* div_element = GetDocument().getElementById(AtomicString("div"));
  div_element->setAttribute(html_names::kStyleAttr,
                            AtomicString("width: 100px; height: 200px;"));
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));

  // Adjust the div's width and height so that it no longer creates a cull rect
  // that clips to a sprite within the image, so the optimization in
  // `ImagePainter::PaintReplaced` does not kick in, and all circles are
  // recorded.
  div_element->removeAttribute(html_names::kStyleAttr);
  Compositor().BeginFrame();
  record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(4U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));
}

// Tests the culling of non-drawing items from a larger sprite sheet.
TEST_F(SVGImageSimTest, SpriteSheetNonDrawingCulling) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<style>"
      "  body { zoom: 2.5; }"
      "  #div {"
      "    width: 100px;"
      "    height: 100px;"
      "    background-image: url(\"data:image/svg+xml,"
      "      <svg xmlns='http://www.w3.org/2000/svg' width='100' height='300'>"
      "        <g mask='url(does_not_exist)'>"
      "          <circle cx='25' cy='50' r='10' fill='red'/>"
      "        </g>"
      "        <g transform='translate(50, 50)'>"
      "          <circle cx='0' cy='0' r='10' fill='red'/>"
      "        </g>"
      "        <g filter='blur(1px)'>"
      "          <circle cx='75' cy='50' r='10' fill='red'/>"
      "        </g>"
      "        <circle cx='50' cy='150' r='10' fill='green'/>"
      "        <g mask='url(does_not_exist)'>"
      "          <circle cx='25' cy='250' r='10' fill='red'/>"
      "        </g>"
      "        <g transform='translate(50, 250)'>"
      "          <circle cx='0' cy='0' r='10' fill='red'/>"
      "        </g>"
      "        <g filter='blur(1px)'>"
      "          <circle cx='75' cy='250' r='10' fill='red'/>"
      "        </g>"
      "      </svg>\");"
      "    background-position-y: -100px;"
      "    background-repeat: no-repeat;"
      "  }"
      "</style>"
      "<div id='div'></div>");

  Compositor().BeginFrame();

  // Only the green circle should be recorded and there should not be any
  // translation paint ops from the <g> elements used to position the red
  // circles.
  PaintRecord record = GetDocument().View()->GetPaintRecord();
  EXPECT_EQ(1U, CountPaintOpType(record, cc::PaintOpType::kDrawOval));
  EXPECT_EQ(0U, CountPaintOpType(record, cc::PaintOpType::kTranslate));
}

}  // namespace blink
