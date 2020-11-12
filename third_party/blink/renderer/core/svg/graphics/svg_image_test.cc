// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
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
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"

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
    scoped_refptr<SharedBuffer> image_data = test::ReadFromFile(file_path);
    EXPECT_TRUE(image_data.get() && image_data.get()->size());

    observer_ = MakeGarbageCollected<PauseControlImageObserver>(true);
    image_ = SVGImage::Create(observer_);
    image_->SetData(image_data, true);
    test::RunPendingTasks();
  }

  void PumpFrame() {
    Image* image = image_.get();
    std::unique_ptr<SkCanvas> null_canvas = SkMakeNullCanvas();
    SkiaPaintCanvas canvas(null_canvas.get());
    PaintFlags flags;
    FloatRect dummy_rect(0, 0, 100, 100);
    image->Draw(&canvas, flags, dummy_rect, dummy_rect,
                kRespectImageOrientation, Image::kDoNotClampImageToSourceRect,
                Image::kSyncDecode);
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
  TaskRunnerTimer<SVGImageChromeClient>* timer =
      new TaskRunnerTimer<SVGImageChromeClient>(
          scheduler::GetSingleThreadTaskRunnerForTesting(), &chrome_client,
          &SVGImageChromeClient::AnimationTimerFired);
  chrome_client.SetTimer(base::WrapUnique(timer));

  // Simulate a draw. Cause a frame (timer) to be scheduled.
  PumpFrame();
  EXPECT_TRUE(GetImage().MaybeAnimated());
  EXPECT_TRUE(timer->IsActive());

  // Fire the timer/trigger a frame update. Since the observer always returns
  // true for shouldPauseAnimation, this will result in the timeline being
  // suspended.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(1) +
                        timer->NextFireInterval());
  EXPECT_TRUE(chrome_client.IsSuspended());
  EXPECT_FALSE(timer->IsActive());

  // Simulate a draw. This should resume the animation again.
  PumpFrame();
  EXPECT_TRUE(timer->IsActive());
  EXPECT_FALSE(chrome_client.IsSuspended());
}

TEST_F(SVGImageTest, ResetAnimation) {
  const bool kShouldPause = false;
  Load(kAnimatedDocument, kShouldPause);
  SVGImageChromeClient& chrome_client = GetImage().ChromeClientForTesting();
  TaskRunnerTimer<SVGImageChromeClient>* timer =
      new TaskRunnerTimer<SVGImageChromeClient>(
          scheduler::GetSingleThreadTaskRunnerForTesting(), &chrome_client,
          &SVGImageChromeClient::AnimationTimerFired);
  chrome_client.SetTimer(base::WrapUnique(timer));

  // Simulate a draw. Cause a frame (timer) to be scheduled.
  PumpFrame();
  EXPECT_TRUE(GetImage().MaybeAnimated());
  EXPECT_TRUE(timer->IsActive());

  // Reset the animation. This will suspend the timeline but not cancel the
  // timer.
  GetImage().ResetAnimation();
  EXPECT_TRUE(chrome_client.IsSuspended());
  EXPECT_TRUE(timer->IsActive());

  // Fire the timer/trigger a frame update. The timeline will remain
  // suspended and no frame will be scheduled.
  test::RunDelayedTasks(base::TimeDelta::FromMillisecondsD(1) +
                        timer->NextFireInterval());
  EXPECT_TRUE(chrome_client.IsSuspended());
  EXPECT_FALSE(timer->IsActive());

  // Simulate a draw. This should resume the animation again.
  PumpFrame();
  EXPECT_FALSE(chrome_client.IsSuspended());
  EXPECT_TRUE(timer->IsActive());
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

  scoped_refptr<SVGImageForContainer> container = SVGImageForContainer::Create(
      &GetImage(), FloatSize(100, 100), 2, NullURL());
  SkBitmap bitmap =
      container->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  ASSERT_EQ(bitmap.width(), 100);
  ASSERT_EQ(bitmap.height(), 100);
  EXPECT_EQ(bitmap.getColor(10, 10), SK_ColorBLUE);
  EXPECT_EQ(bitmap.getColor(90, 10), SK_ColorBLUE);
  EXPECT_EQ(bitmap.getColor(10, 90), SK_ColorBLUE);
  EXPECT_EQ(bitmap.getColor(90, 90), SK_ColorBLUE);
}

class SVGImageSimTest : public SimTest, private ScopedMockOverlayScrollbars {};

TEST_F(SVGImageSimTest, PageVisibilityHiddenToVisible) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.svg",
                                       "image/svg+xml");
  LoadURL("https://example.com/");
  main_resource.Complete("<img src='image.svg' width='100' id='image'>");
  image_resource.Complete(kAnimatedDocument);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  Element* element = GetDocument().getElementById("image");
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
  TimerBase* timer = svg_image_chrome_client.GetTimerForTesting();

  // Wait for the next animation frame to be triggered, and then trigger a new
  // frame. The image animation timeline should be running.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(1) +
                        timer->NextFireInterval());
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());

  // Set page visibility to 'hidden', and then wait for the animation timer to
  // fire. This should suspend the image animation. (Suspend the image's
  // animation timeline.)
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*initial_state=*/false);
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(1) +
                        timer->NextFireInterval());

  EXPECT_TRUE(svg_image_chrome_client.IsSuspended());

  // Set page visibility to 'visible' - this should schedule a new animation
  // frame and resume the image animation.
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*initial_state=*/false);
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(1) +
                        timer->NextFireInterval());
  Compositor().BeginFrame();

  EXPECT_FALSE(svg_image_chrome_client.IsSuspended());
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

}  // namespace blink
