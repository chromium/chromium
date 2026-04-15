// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "base/test/trace_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
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
  enum class ImageStatus { kLoaded, kPending };

  void SetUp() override {
    // Advance the clock so we do not assign null TimeTicks.
    simulated_clock_.Advance(base::Milliseconds(100));
    EnableCompositing();
    RenderingTest::SetUp();

    test_delegate_ = MakeGarbageCollected<LcpTestDelegate>();
    GetLargestContentfulPaintCalculator()->SetDelegateForTest(test_delegate_);
    mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    PaintTiming::From(GetDocument())
        .SetCallbackManagerForTest(mock_callback_manager_);
    // Set this so presentation time callbacks aren't coarsened, which would
    // result in the callback running in a separate task.
    DOMWindowPerformance::performance(*GetDocument().domWindow())
        ->SetCrossOriginIsolatedCapabilityForTesting(true);
    trace_analyzer::Start(kTraceCategories);
  }

  void TearDown() override { RenderingTest::TearDown(); }

  // Note that unlike `PageTestBase::SetBodyInnerHTML`, which this method hides,
  // this does not also simulate a rendering lifecycle update.
  void SetBodyInnerHTML(const String& body_content) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(body_content);
  }

  void SimulateRendering() {
    UpdateAllLifecyclePhasesForTest();
    mock_callback_manager_->OnAnimationFrameComplete();
  }

  void SimulatePresentationTime() {
    mock_callback_manager_->InvokeCallbacksForOneAnimationFrame(
        simulated_clock_.NowTicks());
  }

  void SimulateRenderingAndPresentationTime() {
    SimulateRendering();
    SimulatePresentationTime();
  }

  void SetImage(const char* id,
                int width,
                int height,
                int bytes,
                ImageStatus status = ImageStatus::kLoaded) {
    To<HTMLImageElement>(GetElementById(id))
        ->SetImageForTest(CreateImageForTest(width, height, bytes, status));
  }

  static ImageResourceContent* CreateImageForTest(int width,
                                                  int height,
                                                  int bytes,
                                                  ImageStatus status) {
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
      const bool all_data_received = status == ImageStatus::kLoaded;
      original_image_data->SetData(shared_buffer, all_data_received);
    }
    return status == ImageStatus::kLoaded
               ? ImageResourceContent::CreateLoaded(original_image_data.get())
               : ImageResourceContent::CreatePendingForTest(
                     original_image_data.get());
  }

  uint64_t LargestReportedSize() {
    return test_delegate_->LargestReportedSize();
  }

  uint64_t LargestImagePaintSize() {
    return GetLargestContentfulPaintCalculator()
        ->LatestLcpDetails()
        .largest_image_paint_size;
  }

  base::TimeTicks LargestImagePaintTime() {
    return GetLargestContentfulPaintCalculator()
        ->LatestLcpDetails()
        .largest_image_paint_time;
  }

  double LargestContentfulPaintCandidateImageBPP() {
    return GetLargestContentfulPaintCalculator()
        ->LatestLcpDetails()
        .largest_contentful_paint_image_bpp;
  }

  uint64_t CandidateCount() { return test_delegate_->CandidateCount(); }

  Element* CurrentLcpCandidate() { return test_delegate_->CurrentCandidate(); }

  LargestContentfulPaintCalculator* GetLargestContentfulPaintCalculator() {
    return GetFrame()
        .View()
        ->GetPaintTimingDetector()
        .GetLargestContentfulPaintCalculator();
  }

 private:
  // The tests override the `LargestContentfulPaintCalculator::Delegate` to
  // monitor the stream of candidates.
  class LcpTestDelegate : public GarbageCollected<LcpTestDelegate>,
                          public LargestContentfulPaintCalculator::Delegate {
   public:
    void EmitLcpPerformanceEntry(const DOMPaintTimingInfo& paint_timing_info,
                                 uint64_t paint_size,
                                 base::TimeTicks load_time,
                                 const AtomicString& id,
                                 const String& url,
                                 Element* element) override {
      ++candidate_count_;
      current_candidate_ = element;
      largest_reported_size_ = paint_size;
    }

    void OnLcpMetricsForReportingChanged() override {}

    bool IsHardNavigation() const override { return true; }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(current_candidate_);
    }

    Element* CurrentCandidate() { return current_candidate_; }
    wtf_size_t CandidateCount() { return candidate_count_; }
    uint64_t LargestReportedSize() { return largest_reported_size_; }

   private:
    Member<Element> current_candidate_;
    wtf_size_t candidate_count_ = 0;
    uint64_t largest_reported_size_ = 0;
  };

  base::test::TracingEnvironment tracing_environment_;
  base::SimpleTestTickClock simulated_clock_;
  Persistent<MockPaintTimingCallbackManager> mock_callback_manager_;
  Persistent<LcpTestDelegate> test_delegate_;
};

TEST_F(LargestContentfulPaintCalculatorTest, SingleImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
  )HTML");
  SetImage("target", 100, 150, 1500);
  SimulateRenderingAndPresentationTime();

  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  using trace_analyzer::Query;
  Query q = Query::EventNameIs(kLCPCandidate);
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ(kTraceCategories, events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
  EXPECT_TRUE(arg_dict.FindDouble("imageLoadStart").has_value());
  EXPECT_TRUE(arg_dict.FindDouble("imageLoadEnd").has_value());
  EXPECT_TRUE(arg_dict.FindDouble("imageDiscoveryTime").has_value());

  EXPECT_EQ(LargestReportedSize(), 15000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.8f);
  EXPECT_EQ(CandidateCount(), 1u);
}

TEST_F(LargestContentfulPaintCalculatorTest, SingleText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <p id='text'>This is some text</p>
  )HTML");
  SimulateRenderingAndPresentationTime();

  EXPECT_GT(LargestReportedSize(), 0u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.0f);
  EXPECT_EQ(CandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->GetIdAttribute(), "text");
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageLargerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p id='text'>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3, 100);
  SimulateRenderingAndPresentationTime();

  EXPECT_GT(LargestReportedSize(), 9u);
  EXPECT_EQ(CandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->GetIdAttribute(), "text");
  // The image is still reported to metrics even though it wasn't a web-exposed
  // candidate.
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 800.0f / 9.0f);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageSmallerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200, /*bytes=*/250);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.1f);
  EXPECT_EQ(CandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->GetIdAttribute(), "target");
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
  SimulateRenderingAndPresentationTime();
  // Image is larger than the text.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.08f);
  EXPECT_EQ(CandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->GetIdAttribute(), "large");

  GetDocument().getElementById(AtomicString("large"))->remove();
  SimulateRenderingAndPresentationTime();
  // The LCP does not move after the image is removed.
  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.08f);
  EXPECT_EQ(CandidateCount(), 1u);
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
  SimulateRenderingAndPresentationTime();
  // Text is larger than the image.
  EXPECT_GT(LargestReportedSize(), 50u);
  EXPECT_EQ(CandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->GetIdAttribute(), "large");

  GetDocument().getElementById(AtomicString("large"))->remove();
  SimulateRenderingAndPresentationTime();
  // The LCP should not move after removal.
  EXPECT_GT(LargestReportedSize(), 50u);
  EXPECT_EQ(CandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->GetIdAttribute(), "large");
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, NoPaint) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LargestReportedSize(), 0u);
  EXPECT_EQ(CandidateCount(), 0u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, SingleImageExcludedForEntropy) {
  base::test::ScopedFeatureList scoped_features;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
  )HTML");
  // 600 bytes will cause a calculated entropy of 0.032bpp, which is below the
  // 2bpp threshold.
  SetImage("target", 100, 150, 60);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LargestReportedSize(), 0u);
  EXPECT_EQ(CandidateCount(), 0u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, LargerImageExcludedForEntropy) {
  base::test::ScopedFeatureList scoped_features;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='small'/>
    <img id='large'/>
  )HTML");
  // Smaller image has 1.6 bpp of entropy, enough to be considered for LCP.
  // Larger image has only 0.032 bpp, which is below the 2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 200, 80);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 16.0f);
  EXPECT_EQ(CandidateCount(), 1u);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest,
       LowEntropyImageNotExcludedAtLowerThreshold) {
  base::test::ScopedFeatureList scoped_features;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='small'/>
    <img id='large'/>
  )HTML");
  // Smaller image has 16 bpp of entropy, enough to be considered for LCP.
  // Larger image has 0.32 bpp, which is now above the 0.2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 200, 800);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LargestReportedSize(), 20000u);
  EXPECT_FLOAT_EQ(LargestContentfulPaintCandidateImageBPP(), 0.32f);
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, LargestPendingImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='small' width=3 height=3 />
    <img id='large' width=100 height=300 />
  )HTML");
  // Smaller image has 16 bpp of entropy, enough to be considered for LCP.
  // Larger image has 0.32 bpp, which is now above the 0.2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 300, 800, ImageStatus::kPending);
  SimulateRenderingAndPresentationTime();

  // The smaller image, which is the largest presented image, should be reported
  // to performance timeline, but the UKM value should correspond to the pending
  // image.
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(LargestImagePaintSize(), 30000u);
  EXPECT_TRUE(LargestImagePaintTime().is_null());
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, RemoveLargestPendingImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='small' width=3 height=3 />
    <img id='large' width=100 height=300 />
  )HTML");
  // Smaller image has 16 bpp of entropy, enough to be considered for LCP.
  // Larger image has 0.32 bpp, which is now above the 0.2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 300, 800, ImageStatus::kPending);
  SimulateRenderingAndPresentationTime();

  // The smaller image, which is the largest presented image, should be reported
  // to performance timeline, but the UKM value should correspond to the pending
  // image.
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(LargestImagePaintSize(), 30000u);
  EXPECT_TRUE(LargestImagePaintTime().is_null());

  // Now remove the largest pending image. This should fall back to the largest
  // painted image, but it relies on another contentful paint to trigger the
  // LCP candidate update.
  GetDocument().getElementById(AtomicString("large"))->remove();
  GetLargestContentfulPaintCalculator()->MaybeFlushCandidates();
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(LargestImagePaintSize(), 9u);
  EXPECT_FALSE(LargestImagePaintTime().is_null());
  trace_analyzer::Stop();
}

TEST_F(LargestContentfulPaintCalculatorTest, MulitiplePendingImages) {
  // TODO(crbug.com/466437443): The divs are necessary here to make the images
  // layout vertically since otherwise the (union of the) spaces between images
  // count as text and will be considered an LCP candidate, which we want to
  // avoid in this test. Further complicating this is that the whitespace text
  // lays out differently on Mac and other platforms: on Mac the whitespace text
  // has width 1 and on other platforms it has width 0, which means the text
  // inconsistently counts as an LCP candidate. Excluding whitespace-only text
  // nodes would fix this.
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div><img id='small' width=3 height=3 /></div>
    <div><img id='large' width=100 height=100 /></div>
    <div><img id='largest' width=150 height=200 /></div>
  )HTML");
  // Smaller image has 16 bpp of entropy, enough to be considered for LCP.
  // Larger image has 0.32 bpp, which is now above the 0.2bpp threshold.
  SetImage("small", 3, 3, 18);
  SetImage("large", 100, 100, 800, ImageStatus::kPending);
  SetImage("largest", 150, 200, 800, ImageStatus::kPending);
  SimulateRenderingAndPresentationTime();

  // The smaller image, which is the largest presented image, should be reported
  // to performance timeline, but the UKM value should correspond to the pending
  // image.
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(LargestImagePaintSize(), 30000u);
  EXPECT_TRUE(LargestImagePaintTime().is_null());

  // Now remove the largest pending image. After triggering a candidate update,
  // this should fall back to the largest painted image, not the next largest
  // pending image, which isn't supported.
  GetDocument().getElementById(AtomicString("largest"))->remove();
  GetLargestContentfulPaintCalculator()->MaybeFlushCandidates();
  EXPECT_EQ(LargestReportedSize(), 9u);
  EXPECT_EQ(LargestImagePaintSize(), 9u);
  EXPECT_FALSE(LargestImagePaintTime().is_null());
  trace_analyzer::Stop();
}

}  // namespace blink
