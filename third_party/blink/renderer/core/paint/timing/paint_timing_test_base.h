// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_BASE_H_

#include "base/time/time.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

#define SIMPLE_IMAGE       \
  "data:image/gif;base64," \
  "R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="

#define LARGE_IMAGE                                                            \
  "data:image/gif;base64,"                                                     \
  "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSF" \
  "lzAAAN1wAADdcBQiibeAAAAb5JREFUOMulkr1KA0EQgGdvTwwnYmER0gQsrFKmSy+pLESw9Qm0" \
  "F/ICNnba+h6iEOuAEWslKJKTOyJJvIT72d1xZuOFC0giOLA77O7Mt/PnNptN+I+49Xr9GhH3f3" \
  "mb0v1ht9vtLAUYYw5ItkgDL3KyD8PhcLvdbl/WarXT3DjLMnAcR/f7/YfxeKwtgC5RKQVhGILW" \
  "eg4hQ6hUKjWyucmhLFEUuWR3QYBWAZABQ9i5CCmXy16pVALP80BKaaG+70MQBLvzFMjRKKXh8j" \
  "6FSYKF7ITdEWLa4/ktokN74wiqjSMpnVcbQZqmEJHz+ckeCPFjWKwULpyspAqhdXVXdcnZcPjs" \
  "Ign+2BsVA8jVYuWlgJ3yBj0icgq2uoK+lg4t+ZvLomSKamSQ4AI5BcMADtMhyNoSgNIISUaFNt" \
  "wlazcDcBc4gjjVwCWid2usCWroYEhnaqbzFJLUzAHIXRDChXCcQP8zhkSZ5eNLgHAUzwDcRu4C" \
  "oIRn/wsGUQIIy4Vr9TH6SYFCNzw4nALn5627K4vIttOUOwfa5YnrDYzt/9OLv9I5l8kk5hZ3XL" \
  "O20b7tbR7zHLy/BX8G0IeBEM7ZN1NGIaFUaKLgAAAAAElFTkSuQmCC"

// Mock platform support to provide localized string resources (such as button
// labels) during tests so shadow DOM form controls (e.g. <input type="file">)
// are rendered with non-zero dimensions and recorded by paint timing detectors.
class PaintTimingTestingPlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int message_id) override {
    if (message_id == IDS_FORM_FILE_BUTTON_LABEL ||
        message_id == IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL) {
      return WebString::FromUtf8("Choose File");
    }
    return TestingPlatformSupport::QueryLocalizedString(message_id);
  }
};

// A `PaintTimingClient` that tracks the counts of painted records.
class PaintTimingRecordObserverClient final
    : public GarbageCollected<PaintTimingRecordObserverClient>,
      public PaintTimingClient {
 public:
  void Trace(Visitor* visitor) const override {}

  Type GetType() const override { return Type::kTest; }

  OptionalPaintTimingCallback OnPaintFinished(
      const HeapVector<Member<ImageRecord>>& image_records,
      const HeapVector<Member<TextRecord>>& text_records) override {
    painted_text_record_count_ += text_records.size();
    painted_image_record_count_ += image_records.size();
    return std::nullopt;
  }

  void OnElementFirstContentfulPaint(ImageRecord*) override {
    ++image_first_paint_count_;
  }

  // Returns the total number of text or image records that have been painted.
  wtf_size_t PaintedImageRecordCount() const {
    return painted_image_record_count_;
  }
  wtf_size_t PaintedTextRecordCount() const {
    return painted_text_record_count_;
  }

  // Returns the total number of times `OnElementFirstContentfulPaint()` has
  // been called.
  wtf_size_t ImageFirstPaintCount() const { return image_first_paint_count_; }

 private:
  wtf_size_t painted_image_record_count_ = 0;
  wtf_size_t painted_text_record_count_ = 0;
  wtf_size_t image_first_paint_count_ = 0;
};

// Helper class for getting LCP candidate information in unit tests. Can be used
// as a mixin for test classes, e.g. to support main frame LCP, or separately,
// e.g. to support LCP in iframes.
class LcpTestSupport {
 public:
  LcpTestSupport() = default;

  explicit LcpTestSupport(Document& document) { AttachTo(document); }

  virtual ~LcpTestSupport() = default;

  void AttachTo(Document& document) { document_ = &document; }

  // Returns the current `LargestContentfulPaint`, which is the last candidate
  // emitted to the performance timeline.
  const LargestContentfulPaint* CurrentLcpCandidate() const {
    PerformanceEntryVector entries = GetLcpEntries();
    return entries.empty() ? nullptr
                           : To<LargestContentfulPaint>(entries.back().Get());
  }

  // Returns the number of candidates emitted to the performance timeline.
  wtf_size_t LcpCandidateCount() const { return GetLcpEntries().size(); }

  // Returns the LCP information reported to metrics.
  const LargestContentfulPaintDetailsForReporting& LcpDetailsForReporting()
      const {
    CHECK(document_);
    CHECK(document_->domWindow());
    return DOMWindowPerformance::performance(*document_->domWindow())
        ->timingForReporting()
        ->LargestContentfulPaintDetailsForMetrics();
  }

 private:
  PerformanceEntryVector GetLcpEntries() const {
    CHECK(document_);
    CHECK(document_->domWindow());
    return DOMWindowPerformance::performance(*document_->domWindow())
        ->getBufferedEntriesByType(
            performance_entry_names::kLargestContentfulPaint);
  }

  WeakPersistent<Document> document_;
};

class PaintTimingTestBase : public RenderingTest {
 public:
  PaintTimingTestBase()
      : RenderingTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                      MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  static constexpr base::TimeDelta kQuantumOfTime = base::Milliseconds(10);

  enum class ImageStatus { kLoaded, kPending };

  static ImageResourceContent* CreateImageForTest(
      int width,
      int height,
      int bytes = 0,
      ImageStatus status = ImageStatus::kLoaded) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurfaces::Raster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    scoped_refptr<UnacceleratedStaticBitmapImage> original_image_data =
        UnacceleratedStaticBitmapImage::Create(image);
    if (bytes <= 0) {
      bytes = (width * height / 80) + 1;
    }
    scoped_refptr<SharedBuffer> shared_buffer =
        SharedBuffer::Create(Vector<char>(bytes));
    const bool is_loaded = (status == ImageStatus::kLoaded);
    original_image_data->SetData(shared_buffer, is_loaded);
    return is_loaded
               ? ImageResourceContent::CreateLoaded(original_image_data.get())
               : ImageResourceContent::CreatePendingForTest(
                     original_image_data.get());
  }

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();

    if (GetDocument().GetSettings()) {
      // Disable media controls to prevent default media control shadow elements
      // from creating unexpected LCP candidate entries during video tests.
      GetDocument().GetSettings()->SetMediaControlsEnabled(false);
    }

    // Advance clock so initial time is non-zero (avoids rendering assertions).
    AdvanceClock(base::Milliseconds(1));

    mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    PaintTiming::From(GetDocument())
        .SetCallbackManagerForTest(mock_callback_manager_);
    CHECK(GetDocument().domWindow());
    DOMWindowPerformance::performance(*GetDocument().domWindow())
        ->SetCrossOriginIsolatedCapabilityForTesting(true);
  }

  void TearDown() override {
    MemoryCache::Get()->EvictResources();
    mock_callback_manager_->Shutdown();
    RenderingTest::TearDown();
  }

  // Sets the main frame document's body content. Does not cause a rendering
  // update.
  void SetMainFrameBodyContent(const String& content) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(content);
    CHECK(GetDocument().domWindow());
    DOMWindowPerformance::performance(*GetDocument().domWindow())
        ->SetCrossOriginIsolatedCapabilityForTesting(true);
  }

  // Sets the child frame document's body content. Does not cause a rendering
  // update.
  void SetChildFrameBodyContent(const String& content) {
    SetChildFrameHTML(content);
    PaintTiming::From(ChildDocument())
        .SetCallbackManagerForTest(mock_callback_manager_);
    CHECK(ChildDocument().domWindow());
    DOMWindowPerformance::performance(*ChildDocument().domWindow())
        ->SetCrossOriginIsolatedCapabilityForTesting(true);
  }

  void SimulateRendering() {
    UpdateAllLifecyclePhasesForTest();
    mock_callback_manager_->OnAnimationFrameComplete();
  }

  void SimulatePresentationTime() {
    AdvanceClock(kQuantumOfTime);
    mock_callback_manager_->OnAnimationFramePresented(base::TimeTicks::Now());
    mock_callback_manager_->InvokeCallbacksForNextAnimationFrame();
  }

  void SimulateRenderingAndPresentationTime() {
    SimulateRendering();
    SimulatePresentationTime();
  }

  // Returns the `MockPaintTimingCallbackManager` controlling presentation
  // callbacks. Tests should not typically need this and should instead use
  // `SimulateRendering` and `SimulatePresentationTime()`, but this can be used
  // for advanced cases, e.g. simulating out-of-order presentation feedback.
  MockPaintTimingCallbackManager* GetMockPaintTimingCallbackManager() {
    return mock_callback_manager_.Get();
  }

  void SimulatePassOfTime() { AdvanceClock(kQuantumOfTime); }

  PaintTiming& GetPaintTiming() { return PaintTiming::From(GetDocument()); }

  PaintTimingDetector& GetPaintTimingDetector() {
    return PaintTimingDetector::From(GetDocument());
  }

  gfx::Rect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect(kExcludeScrollbars);
  }

  void SimulateScroll(
      mojom::blink::ScrollType type = mojom::blink::ScrollType::kUser) {
    GetPaintTiming().NotifyScroll(type);
  }

  void SimulateKeyDown() {
    GetPaintTiming().NotifyInputEvent(WebInputEvent::Type::kKeyDown);
  }

  void SimulateKeyUp() {
    GetPaintTiming().NotifyInputEvent(WebInputEvent::Type::kKeyUp);
  }

  // Sets the image content for the given `id`, which must be an `ImageElement`
  // or `SVGImageElement`. Returns the corresponding `ImageResourceContent`.
  ImageResourceContent* SetImageContent(
      const char* id,
      int width,
      int height,
      int bytes = 0,
      ImageStatus status = ImageStatus::kLoaded) {
    return SetImageContentImpl(GetElementById(id), width, height, bytes,
                               status);
  }

  ImageResourceContent* SetChildFrameImageContent(
      const char* id,
      int width,
      int height,
      int bytes = 0,
      ImageStatus status = ImageStatus::kLoaded) {
    return SetImageContentImpl(ChildDocument().getElementById(AtomicString(id)),
                               width, height, bytes, status);
  }

  // Creates an `ImageResource` for `url` and adds it to `MemoryCache` so CSS
  // `url(...)` references (e.g. `background-image`) resolve to it in tests.
  ImageResourceContent* AddImageToMemoryCache(const char* url,
                                              int width,
                                              int height,
                                              int bytes,
                                              ImageStatus status) {
    ImageResourceContent* content =
        CreateImageForTest(width, height, bytes, status);
    ResourceRequest request{KURL(url)};
    request.SetRequestorOrigin(
        GetDocument().GetExecutionContext()->GetSecurityOrigin());
    auto* image_resource = MakeGarbageCollected<ImageResource>(
        request, ResourceLoaderOptions(/*world_for_csp=*/nullptr), content);
    image_resource->SetStatus(status == ImageStatus::kLoaded
                                  ? ResourceStatus::kCached
                                  : ResourceStatus::kPending);
    image_resource->SetCacheIdentifier(MemoryCache::DefaultCacheIdentifier());
    MemoryCache::Get()->Add(image_resource);
    return content;
  }

 private:
  ImageResourceContent* SetImageContentImpl(
      Element* element,
      int width,
      int height,
      int bytes = 0,
      ImageStatus status = ImageStatus::kLoaded) {
    ImageResourceContent* content =
        CreateImageForTest(width, height, bytes, status);
    if (auto* image = DynamicTo<HTMLImageElement>(element)) {
      image->SetImageForTest(content);
    } else if (auto* svg_image = DynamicTo<SVGImageElement>(element)) {
      svg_image->SetImageForTest(content);
    } else {
      NOTREACHED();
    }
    return content;
  }

  ScopedTestingPlatformSupport<PaintTimingTestingPlatformSupport> platform_;
  Persistent<MockPaintTimingCallbackManager> mock_callback_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_BASE_H_
