// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"

#include "base/test/simple_test_tick_clock.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing_test_helper.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class LargestContentfulPaintCalculatorTest : public RenderingTest {
 public:
  void SetUp() override {
    // Advance the clock so we do not assign null TimeTicks.
    simulated_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
    EnableCompositing();
    RenderingTest::SetUp();

    mock_text_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetTextPaintTimingDetector()->ResetCallbackManager(
        mock_text_callback_manager_);
    mock_image_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetImagePaintTimingDetector()->ResetCallbackManager(
        mock_image_callback_manager_);
    trace_event::EnableTracing(TRACE_DISABLED_BY_DEFAULT("loading"));
  }

  ImagePaintTimingDetector* GetImagePaintTimingDetector() {
    return GetFrame()
        .View()
        ->GetPaintTimingDetector()
        .GetImagePaintTimingDetector();
  }
  TextPaintTimingDetector* GetTextPaintTimingDetector() {
    return GetFrame()
        .View()
        ->GetPaintTimingDetector()
        .GetTextPaintTimingDetector();
  }

  void SetImage(const char* id, int width, int height) {
    To<HTMLImageElement>(GetDocument().getElementById(id))
        ->SetImageForTest(CreateImageForTest(width, height));
  }

  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_content =
        ImageResourceContent::CreateLoaded(
            UnacceleratedStaticBitmapImage::Create(image).get());
    return original_image_content;
  }

  uint64_t LargestReportedSize() {
    return GetLargestContentfulPaintCalculator()->largest_reported_size_;
  }

  uint64_t CountCandidates() {
    return GetLargestContentfulPaintCalculator()->count_candidates_;
  }

  void UpdateLargestContentfulPaintCandidate() {
    GetFrame()
        .View()
        ->GetPaintTimingDetector()
        .UpdateLargestContentfulPaintCandidate();
  }

  void SimulateContentSwapPromise() {
    mock_text_callback_manager_->InvokeSwapTimeCallback(
        simulated_clock_.NowTicks());
    mock_image_callback_manager_->InvokeSwapTimeCallback(
        simulated_clock_.NowTicks());
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    UpdateLargestContentfulPaintCandidate();
  }

  // Outside the tests, the text callback and the image callback are run
  // together, as in |SimulateContentSwapPromise|.
  void SimulateImageSwapPromise() {
    mock_image_callback_manager_->InvokeSwapTimeCallback(
        simulated_clock_.NowTicks());
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    UpdateLargestContentfulPaintCandidate();
  }

  // Outside the tests, the text callback and the image callback are run
  // together, as in |SimulateContentSwapPromise|.
  void SimulateTextSwapPromise() {
    mock_text_callback_manager_->InvokeSwapTimeCallback(
        simulated_clock_.NowTicks());
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    UpdateLargestContentfulPaintCandidate();
  }

 private:
  LargestContentfulPaintCalculator* GetLargestContentfulPaintCalculator() {
    return GetFrame()
        .View()
        ->GetPaintTimingDetector()
        .GetLargestContentfulPaintCalculator();
  }

  base::SimpleTestTickClock simulated_clock_;
  Persistent<MockPaintTimingCallbackManager> mock_text_callback_manager_;
  Persistent<MockPaintTimingCallbackManager> mock_image_callback_manager_;
};

TEST_F(LargestContentfulPaintCalculatorTest, SingleImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
  )HTML");
  SetImage("target", 100, 150);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();

  EXPECT_EQ(LargestReportedSize(), 15000u);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, SingleText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <p>This is some text</p>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  SimulateTextSwapPromise();

  EXPECT_GT(LargestReportedSize(), 0u);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageLargerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(CountCandidates(), 1u);
  SimulateTextSwapPromise();

  EXPECT_GT(LargestReportedSize(), 9u);
  EXPECT_EQ(CountCandidates(), 2u);
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageSmallerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);
  SimulateTextSwapPromise();

  // Text should not be reported, since it is smaller than the image.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, TextLargerImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200);
  UpdateAllLifecyclePhasesForTest();
  SimulateContentSwapPromise();

  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, TextSmallerImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3);
  UpdateAllLifecyclePhasesForTest();
  SimulateContentSwapPromise();

  // Image should not be reported, since it is smaller than the text.
  EXPECT_GT(LargestReportedSize(), 9u);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, LargestImageRemoved) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='large'/>
    <img id='small'/>
    <p>Larger than the second image</p>
  )HTML");
  SetImage("large", 100, 200);
  SetImage("small", 3, 3);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  SimulateTextSwapPromise();
  // Image is larger than the text.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);

  GetDocument().getElementById("large")->remove();
  UpdateAllLifecyclePhasesForTest();
  // The LCP does not move after the text is removed.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, LargestTextRemoved) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='medium'/>
    <p id='large'>
      This text element should be larger than than the image!\n
      These words ensure that this is the case.\n
      But the image will be larger than the other paragraph!
    </p>
    <p id='small'>.</p>
  )HTML");
  SetImage("medium", 10, 5);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  SimulateTextSwapPromise();
  // Test is larger than the image.
  EXPECT_GT(LargestReportedSize(), 50u);
  // Image swap occurred first, so we have would have two candidates.
  EXPECT_EQ(CountCandidates(), 2u);

  GetDocument().getElementById("large")->remove();
  UpdateAllLifecyclePhasesForTest();
  // The LCP should not move after removal.
  EXPECT_GT(LargestReportedSize(), 50u);
  EXPECT_EQ(CountCandidates(), 2u);
}

}  // namespace blink
