// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_BASE_H_

#include "base/time/time.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

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

// A `PaintTimingClient` that tracks the number of `PaintTimingRecord`s waiting
// for presentation time by observing paint and presentation for each record.
class PendingPaintTimingRecordObserverClient
    : public GarbageCollected<PendingPaintTimingRecordObserverClient>,
      public PaintTimingClient {
 public:
  void Trace(Visitor* visitor) const override {
    visitor->Trace(pending_text_records_);
    visitor->Trace(pending_image_records_);
  }

  void OnElementLastContentfulPaint(TextRecord* record,
                                    bool was_previously_presented) override {
    // Only track records that are needed by some other client. This works
    // because this observer is added after the default clients, specifically
    // LCP for these tests.
    if (record->IsNeededForPaintTiming()) {
      pending_text_records_.insert(record);
    }
  }

  void OnElementLastContentfulPaint(ImageRecord* record) override {
    // Only track records that are needed by some other client. This works
    // because this observer is added after the default clients, specifically
    // LCP for these tests.
    if (record->IsNeededForPaintTiming()) {
      pending_image_records_.insert(record);
    }
  }

  void OnFramePresented(const HeapVector<Member<ImageRecord>>& image_records,
                        const HeapVector<Member<TextRecord>>& text_records,
                        const HeapVector<Member<ElementTimingInfo>>&,
                        const DOMPaintTimingInfo&) override {
    for (TextRecord* record : text_records) {
      pending_text_records_.erase(record);
    }
    for (ImageRecord* record : image_records) {
      pending_image_records_.erase(record);
    }
  }

  wtf_size_t PendingTextRecordsSize() { return pending_text_records_.size(); }
  wtf_size_t PendingImageRecordsSize() { return pending_image_records_.size(); }

 private:
  HeapHashSet<Member<TextRecord>> pending_text_records_;
  HeapHashSet<Member<ImageRecord>> pending_image_records_;
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
    mock_callback_manager_->InvokeCallbacksForOneAnimationFrame(NowTicks());
  }

  void SimulateRenderingAndPresentationTime() {
    SimulateRendering();
    SimulatePresentationTime();
  }

  void SimulatePassOfTime() { AdvanceClock(kQuantumOfTime); }

  base::TimeTicks NowTicks() { return base::TimeTicks::Now(); }

  PaintTiming& GetPaintTiming() { return PaintTiming::From(GetDocument()); }

  PaintTiming& GetChildFramePaintTiming() {
    return PaintTiming::From(ChildDocument());
  }

  PaintTimingDetector& GetPaintTimingDetector() {
    return PaintTimingDetector::From(GetDocument());
  }

  PaintTimingDetector& GetChildPaintTimingDetector() {
    return PaintTimingDetector::From(ChildDocument());
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

  // Sets the image content the given `id`, which must be an `ImageElement` or
  // `SVGImageElement`. Returns the corresponding `ImageResourceContent`.
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
