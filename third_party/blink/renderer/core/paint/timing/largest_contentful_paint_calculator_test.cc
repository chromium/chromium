// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_helper.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {
constexpr const char kTraceCategories[] = "loading,rail,devtools.timeline";

constexpr const char kLCPCandidate[] = "largestContentfulPaint::Candidate";
}  // namespace

class LargestContentfulPaintCalculatorTest : public RenderingTest {
 public:
  void SetUp() override {
    // Advance the clock so we do not assign null TimeTicks.
    simulated_clock_.Advance(base::Milliseconds(100));
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
    trace_analyzer::Start(kTraceCategories);
  }

  void TearDown() override { RenderingTest::TearDown(); }

  ImagePaintTimingDetector* GetImagePaintTimingDetector() {
    return &GetFrame()
                .View()
                ->GetPaintTimingDetector()
                .GetImagePaintTimingDetector();
  }
  TextPaintTimingDetector* GetTextPaintTimingDetector() {
    return &GetFrame()
                .View()
                ->GetPaintTimingDetector()
                .GetTextPaintTimingDetector();
  }

  void SetImage(const char* id, int width, int height, int bytes = 0) {
    To<HTMLImageElement>(GetElementById(id))
        ->SetImageForTest(CreateImageForTest(width, height, bytes));
  }

  ImageResourceContent* CreateImageForTest(int width,
                                           int height,
                                           int bytes = 0) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurfaces::Raster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    scoped_refptr<UnacceleratedStaticBitmapImage> original_image_data =
        UnacceleratedStaticBitmapImage::Create(image);
    // If a byte size is specified, then also assign a suitably-sized
    // vector of 0s to the image. This is used for bits-per-pixel
    // calculations.
    if (bytes > 0) {
      scoped_refptr<SharedBuffer> shared_buffer =
          SharedBuffer::Create(Vector<char>(bytes));
      original_image_data->SetData(shared_buffer, /*all_data_received=*/true);
    }
    ImageResourceContent* original_image_content =
        ImageResourceContent::CreateLoaded(original_image_data.get());
    return original_image_content;
  }

  uint64_t LargestReportedSize() {
    return GetLargestContentfulPaintCalculator()->largest_reported_size_;
  }

  double LargestContentfulPaintCandidateImageBPP() {
    return GetLargestContentfulPaintCalculator()->largest_image_bpp_;
  }

  uint64_t CountCandidates() {
    return GetLargestContentfulPaintCalculator()->count_candidates_;
  }

  void UpdateLargestContentfulPaintCandidate() {
    GetFrame().View()->GetPaintTimingDetector().UpdateLcpCandidate();
  }

  void SimulateContentPresentationPromise() {
    mock_text_callback_manager_->InvokePresentationTimeCallback(
        simulated_clock_.NowTicks());
    mock_image_callback_manager_->InvokePresentationTimeCallback(
        simulated_clock_.NowTicks());
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    UpdateLargestContentfulPaintCandidate();
  }

  // Outside the tests, the text callback and the image callback are run
  // together, as in |SimulateContentPresentationPromise|.
  void SimulateImagePresentationPromise() {
    mock_image_callback_manager_->InvokePresentationTimeCallback(
        simulated_clock_.NowTicks());
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    UpdateLargestContentfulPaintCandidate();
  }

  // Outside the tests, the text callback and the image callback are run
  // together, as in |SimulateContentPresentationPromise|.
  void SimulateTextPresentationPromise() {
    mock_text_callback_manager_->InvokePresentationTimeCallback(
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
  SetImage("target", 100, 150, 1500);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();

  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  using trace_analyzer::Query;
  Query q = Query::EventNameIs(kLCPCandidate);
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(kTraceCategories, events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");
  EXPECT_TRUE(arg_dict.FindDouble("imageLoadStart").has_value());
  EXPECT_TRUE(arg_dict.FindDouble("imageLoadEnd").has_value());
  EXPECT_TRUE(arg_dict.FindDouble("imageDiscoveryTime").has_value());

  EXPECT_EQ(LargestReportedSize(), 15000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.8f);
  EXPECT_EQ(CountCandidates(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, SingleText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <p>This is some text</p>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  SimulateTextPresentationPromise();

  EXPECT_GT(LargestReportedSize(), 0u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.0f);
  EXPECT_EQ(CountCandidates(), 1u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageLargerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3, 100);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(CountCandidates(), 1u);
  SimulateTextPresentationPromise();

  EXPECT_GT(LargestReportedSize(), 9u);
  EXPECT_EQ(CountCandidates(), 2u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageSmallerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200, /*bytes=*/250);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);
  SimulateTextPresentationPromise();

  // Text should not be reported, since it is smaller than the image.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.1f);
  EXPECT_EQ(CountCandidates(), 1u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, TextLargerImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200, /*bytes=*/250);
  UpdateAllLifecyclePhasesForTest();
  SimulateContentPresentationPromise();

  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_EQ(CountCandidates(), 1u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, TextSmallerImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3, /*bytes=*/9);
  UpdateAllLifecyclePhasesForTest();
  SimulateContentPresentationPromise();

  // Image should not be reported, since it is smaller than the text. No image
  // BPP should be recorded.
  EXPECT_GT(LargestReportedSize(), 9u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.0f);
  EXPECT_EQ(CountCandidates(), 1u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, LargestImageRemoved) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='large'/>
    <img id='small'/>
    <p>Larger than the second image</p>
  )HTML");
  SetImage("large", 100, 200, 200);
  SetImage("small", 3, 3, 18);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();
  SimulateTextPresentationPromise();
  // Image is larger than the text.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.08f);
  EXPECT_EQ(CountCandidates(), 1u);

  GetDocument().getElementById(AtomicString("large"))->remove();
  UpdateAllLifecyclePhasesForTest();
  // The LCP does not move after the image is removed.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.08f);
  EXPECT_EQ(CountCandidates(), 1u);
  trace_analyzer::Stop();
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
  SetImage("medium", 10, 5, /*bytes=*/50);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();
  SimulateTextPresentationPromise();
  // Test is larger than the image.
  EXPECT_GT(LargestReportedSize(), 50u);
  // Image presentation occurred first, so we have would have two candidates.
  EXPECT_EQ(CountCandidates(), 2u);

  GetDocument().getElementById(AtomicString("large"))->remove();
  UpdateAllLifecyclePhasesForTest();
  // The LCP should not move after removal.
  EXPECT_GT(LargestReportedSize(), 50u);
  EXPECT_EQ(CountCandidates(), 2u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, NoPaint) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  UpdateLargestContentfulPaintCandidate();
  EXPECT_EQ(LargestReportedSize(), 0u);
  EXPECT_EQ(CountCandidates(), 0u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, SingleImageExcludedForEntropy) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      blink::features::kExcludeLowEntropyImagesFromLCP, {{"min_bpp", "2.0"}});
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
  )HTML");
  // 600 bytes will cause a calculated entropy of 0.32bpp, which is below the
  // 2bpp threshold.
  SetImage("target", 100, 150, 600);
  UpdateAllLifecyclePhasesForTest();
  UpdateLargestContentfulPaintCandidate();

  EXPECT_EQ(LargestReportedSize(), 0u);
  EXPECT_EQ(CountCandidates(), 0u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, LargerImageExcludedForEntropy) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      blink::features::kExcludeLowEntropyImagesFromLCP, {{"min_bpp", "2.0"}});
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='small'/>
    <img id='large'/>
  )HTML");
  // Smaller image has 16 bpp of entropy, enough to be considered for LCP.
  // Larger image has only 0.32 bpp, which is below the 2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 200, 800);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();

  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 16.0f);
  EXPECT_EQ(CountCandidates(), 1u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest,
       LowEntropyImageNotExcludedAtLowerThreshold) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      blink::features::kExcludeLowEntropyImagesFromLCP, {{"min_bpp", "0.02"}});
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='small'/>
    <img id='large'/>
  )HTML");
  // Smaller image has 16 bpp of entropy, enough to be considered for LCP.
  // Larger image has 0.32 bpp, which is now above the 0.2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 200, 800);
  UpdateAllLifecyclePhasesForTest();
  SimulateImagePresentationPromise();

  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.32f);
  trace_analyzer::Stop();
}

}  // namespace blink
