// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_input_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

namespace blink {

namespace {

const char* kControlInputElementHistogramName =
    "Media.Controls.CTR.MediaControlInputElementImpl";
const char* kControlInputElementOverflowHistogramName =
    "Media.Controls.CTR.MediaControlInputElementImplOverflow";

// Minimalist implementation of the MediaControlInputElement interface in order
// to be able to test it.
class MediaControlInputElementImpl final : public MediaControlInputElement {
 public:
  MediaControlInputElementImpl(MediaControlsImpl& media_controls)
      : MediaControlInputElement(media_controls) {
    setType(input_type_names::kButton);
    SetIsWanted(false);
  }

  void Trace(blink::Visitor* visitor) override {
    MediaControlInputElement::Trace(visitor);
  }

 protected:
  const char* GetNameForHistograms() const final {
    return IsOverflowElement() ? "MediaControlInputElementImplOverflow"
                               : "MediaControlInputElementImpl";
  }

  int GetOverflowStringId() const final {
    return IDS_MEDIA_OVERFLOW_MENU_DOWNLOAD;
  }
};

}  // anonymous namespace

class MediaControlInputElementTest : public PageTestBase {
 public:
  void SetUp() final {
    // Create page and add a video element with controls.
    PageTestBase::SetUp();
    media_element_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    media_element_->SetBooleanAttribute(html_names::kControlsAttr, true);
    GetDocument().body()->AppendChild(media_element_);

    // Create instance of MediaControlInputElement to run tests on.
    media_controls_ =
        static_cast<MediaControlsImpl*>(media_element_->GetMediaControls());
    ASSERT_NE(media_controls_, nullptr);
    control_input_element_ =
        MakeGarbageCollected<MediaControlInputElementImpl>(*media_controls_);
  }

 protected:
  void MaybeRecordInteracted() {
    control_input_element_->MaybeRecordInteracted();
  }

  void SetReadyState(HTMLMediaElement::ReadyState ready_state) {
    media_element_->SetReadyState(ready_state);
  }

  MediaControlInputElementImpl& ControlInputElement() {
    return *control_input_element_;
  }

  MediaControlsImpl& MediaControls() { return *media_controls_; }

  HTMLMediaElement& MediaElement() { return *media_element_; }

 private:
  Persistent<HTMLMediaElement> media_element_;
  Persistent<MediaControlsImpl> media_controls_;
  Persistent<MediaControlInputElementImpl> control_input_element_;
};

TEST_F(MediaControlInputElementTest, MaybeRecordDisplayed_IfNotWantedOrNoFit) {
  HistogramTester histogram_tester_;

  ControlInputElement().SetIsWanted(false);
  ControlInputElement().SetDoesFit(false);
  ControlInputElement().MaybeRecordDisplayed();

  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(false);
  ControlInputElement().MaybeRecordDisplayed();

  ControlInputElement().SetIsWanted(false);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();

  histogram_tester_.ExpectTotalCount(kControlInputElementHistogramName, 0);
}

TEST_F(MediaControlInputElementTest, MaybeRecordDisplayed_WantedAndFit) {
  HistogramTester histogram_tester_;

  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();

  histogram_tester_.ExpectUniqueSample(kControlInputElementHistogramName, 0, 1);
}

TEST_F(MediaControlInputElementTest, MaybeRecordDisplayed_TwiceDoesNotRecord) {
  HistogramTester histogram_tester_;

  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();
  ControlInputElement().MaybeRecordDisplayed();

  histogram_tester_.ExpectUniqueSample(kControlInputElementHistogramName, 0, 1);
}

TEST_F(MediaControlInputElementTest, MaybeRecordInteracted_Basic) {
  HistogramTester histogram_tester_;

  // The element has to be displayed first.
  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();

  MaybeRecordInteracted();

  histogram_tester_.ExpectTotalCount(kControlInputElementHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kControlInputElementHistogramName, 0, 1);
  histogram_tester_.ExpectBucketCount(kControlInputElementHistogramName, 1, 1);
}

TEST_F(MediaControlInputElementTest, MaybeRecordInteracted_TwiceDoesNotRecord) {
  HistogramTester histogram_tester_;

  // The element has to be displayed first.
  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();

  MaybeRecordInteracted();
  MaybeRecordInteracted();

  histogram_tester_.ExpectTotalCount(kControlInputElementHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kControlInputElementHistogramName, 0, 1);
  histogram_tester_.ExpectBucketCount(kControlInputElementHistogramName, 1, 1);
}

TEST_F(MediaControlInputElementTest, ClickRecordsInteraction) {
  HistogramTester histogram_tester_;

  // The element has to be displayed first.
  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();

  ControlInputElement().DispatchSimulatedClick(
      Event::CreateBubble(event_type_names::kClick), kSendNoEvents);

  histogram_tester_.ExpectTotalCount(kControlInputElementHistogramName, 2);
  histogram_tester_.ExpectBucketCount(kControlInputElementHistogramName, 0, 1);
  histogram_tester_.ExpectBucketCount(kControlInputElementHistogramName, 1, 1);
}

TEST_F(MediaControlInputElementTest, OverflowElement_DisplayFallback) {
  HistogramTester histogram_tester_;

  Persistent<HTMLElement> overflow_container =
      ControlInputElement().CreateOverflowElement(
          MakeGarbageCollected<MediaControlInputElementImpl>(MediaControls()));

  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(false);
  ControlInputElement().SetOverflowElementIsWanted(true);
  ControlInputElement().MaybeRecordDisplayed();

  histogram_tester_.ExpectTotalCount(kControlInputElementHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kControlInputElementOverflowHistogramName, 0, 1);
}

TEST_F(MediaControlInputElementTest, OverflowElement_DisplayRequiresWanted) {
  HistogramTester histogram_tester_;

  Persistent<HTMLElement> overflow_container =
      ControlInputElement().CreateOverflowElement(
          MakeGarbageCollected<MediaControlInputElementImpl>(MediaControls()));

  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(false);
  ControlInputElement().SetOverflowElementIsWanted(false);
  ControlInputElement().MaybeRecordDisplayed();

  ControlInputElement().SetIsWanted(false);
  ControlInputElement().SetDoesFit(false);
  ControlInputElement().SetOverflowElementIsWanted(true);
  ControlInputElement().MaybeRecordDisplayed();

  histogram_tester_.ExpectTotalCount(kControlInputElementHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kControlInputElementOverflowHistogramName,
                                     0);
}

TEST_F(MediaControlInputElementTest, OverflowElement_DisplayAfterInline) {
  HistogramTester histogram_tester_;

  Persistent<HTMLElement> overflow_container =
      ControlInputElement().CreateOverflowElement(
          MakeGarbageCollected<MediaControlInputElementImpl>(MediaControls()));

  ControlInputElement().SetIsWanted(true);
  ControlInputElement().SetDoesFit(true);
  ControlInputElement().MaybeRecordDisplayed();

  ControlInputElement().SetDoesFit(false);
  ControlInputElement().SetOverflowElementIsWanted(true);
  ControlInputElement().MaybeRecordDisplayed();

  histogram_tester_.ExpectUniqueSample(kControlInputElementHistogramName, 0, 1);
  histogram_tester_.ExpectUniqueSample(
      kControlInputElementOverflowHistogramName, 0, 1);
}

TEST_F(MediaControlInputElementTest, ShouldRecordDisplayStates_ReadyState) {
  MediaElement().setAttribute(html_names::kPreloadAttr, "auto");

  SetReadyState(HTMLMediaElement::kHaveNothing);
  EXPECT_FALSE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));

  SetReadyState(HTMLMediaElement::kHaveMetadata);
  EXPECT_TRUE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));

  SetReadyState(HTMLMediaElement::kHaveCurrentData);
  EXPECT_TRUE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));

  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_TRUE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  EXPECT_TRUE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));
}

TEST_F(MediaControlInputElementTest, ShouldRecordDisplayStates_Preload) {
  // Set ready state to kHaveNothing to make sure only the preload state impacts
  // the result.
  SetReadyState(HTMLMediaElement::kHaveNothing);

  MediaElement().setAttribute(html_names::kPreloadAttr, "none");
  EXPECT_TRUE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));

  MediaElement().setAttribute(html_names::kPreloadAttr, "preload");
  EXPECT_FALSE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));

  MediaElement().setAttribute(html_names::kPreloadAttr, "auto");
  EXPECT_FALSE(
      MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()));
}

}  // namespace blink
