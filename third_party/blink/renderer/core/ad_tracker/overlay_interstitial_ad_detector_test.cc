// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/overlay_interstitial_ad_detector.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class OverlayInterstitialAdDetectorTest : public RenderingTest {
 protected:
  OverlayInterstitialAdDetectorTest() {
    // Let every lifecycle update run a detection.
    scoped_feature_list_.InitAndDisableFeature(
        features::kFrequencyCappingForOverlayPopupDetection);
  }

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void SimulateFirstContentfulPaint() {
    SimulateFirstContentfulPaintAt(base::TimeTicks::Now());
  }

  void SimulateFirstContentfulPaintAt(base::TimeTicks presentation_time) {
    viz::FrameTimingDetails details;
    details.presentation_feedback.timestamp = presentation_time;
    PaintTiming::From(GetDocument())
        .ReportPresentationTime(PaintEvent::kFirstContentfulPaint,
                                base::TimeTicks(), details);
  }

  // Sets up a page whose viewport center is covered by a large, fixed-position
  // ad element. With `scrollable`, the page also gets enough in-flow content
  // to scroll, so the detector decides on the overlay at its dismissal instead
  // of at its appearance. Returns the ad element, which is initially hidden if
  // `ad_hidden` is set.
  Element* SetUpPageWithOverlayAd(bool scrollable, bool ad_hidden) {
    SetBodyContent(base::StrCat({R"HTML(
      <style>
        #ad {
          position: fixed;
          left: 0;
          top: 0;
          width: 100vw;
          height: 100vh;
          display: )HTML",
                                 ad_hidden ? "none" : "block", R"HTML(;
        }
        #content { height: )HTML",
                                 scrollable ? "5000px" : "10px", R"HTML(; }
      </style>
      <div id="content">content</div>
      <div id="ad"></div>
    )HTML"}));
    Element* ad = GetElementById("ad");
    ad->SetIsAdRelated(NoProvenance{});
    return ad;
  }

  void SetAdDisplayed(Element* ad, bool displayed) {
    ad->SetInlineStyleProperty(
        CSSPropertyID::kDisplay,
        displayed ? CSSValueID::kBlock : CSSValueID::kNone);
  }

  // Scrolls the main frame as the user would: besides moving the scroll
  // offset, the scroll is reported to the InteractiveDetector as the compositor
  // does once the frame for a scroll input event has been presented.
  void SimulateUserScrollTo(int y) {
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, y), mojom::blink::ScrollType::kUser,
        cc::ScrollSourceType::kNone);
    InteractiveDetector::From(GetDocument())
        ->DidObserveFirstScrollDelay(base::Milliseconds(10),
                                     base::TimeTicks::Now());
  }

  // Scrolls the main frame without any user input, e.g. as scrollTo() would.
  void ScrollMainFrameProgrammaticallyTo(int y) {
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, y), mojom::blink::ScrollType::kProgrammatic,
        cc::ScrollSourceType::kNone);
  }

  void SetPageVisibility(mojom::blink::PageVisibilityState state) {
    GetPage().SetVisibilityState(state, /*is_initial_state=*/false);
  }

  bool IsUseCounted(WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test: MaybeFireDetection should not crash when the document
// lifecycle has not yet reached kPrePaintClean.
TEST_F(OverlayInterstitialAdDetectorTest,
       MaybeFireDetectionSkipsWhenNotPrePaintClean) {
  SetBodyInnerHTML("<div>content</div>");
  UpdateAllLifecyclePhasesForTest();
  SimulateFirstContentfulPaint();
  ASSERT_FALSE(
      PaintTiming::From(GetDocument()).FirstContentfulPaint().is_null());

  GetDocument().View()->SetNeedsLayout();
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  ASSERT_EQ(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);

  // Without the lifecycle guard this would DCHECK in PaintLayer::HitTestLayer.
  OverlayInterstitialAdDetector detector;
  detector.MaybeFireDetection(&GetDocument().GetFrame()->LocalFrameRoot());
}

// An overlay ad that is already showing at the first contentful paint is a
// prestitial, which is counted both as a pop-up ad and as a prestitial ad, and
// the element is tagged as both for the ad highlight label.
TEST_F(OverlayInterstitialAdDetectorTest, PrestitialAdOnUnscrollablePage) {
  Element* ad =
      SetUpPageWithOverlayAd(/*scrollable=*/false, /*ad_hidden=*/false);
  SimulateFirstContentfulPaint();

  // The page is not scrollable, so the overlay is decided on at appearance.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopup));
  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
  EXPECT_TRUE(ad->IsOverlayPopupAd());
  EXPECT_TRUE(ad->IsPrestitialAd());
}

// On a scrollable page the overlay is decided on at dismissal time; the
// prestitial conditions captured at its appearance must carry over.
TEST_F(OverlayInterstitialAdDetectorTest, PrestitialAdOnScrollablePage) {
  Element* ad =
      SetUpPageWithOverlayAd(/*scrollable=*/true, /*ad_hidden=*/false);
  SimulateFirstContentfulPaint();

  // The overlay becomes a candidate.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_FALSE(ad->IsOverlayPopupAd());
  EXPECT_FALSE(ad->IsPrestitialAd());

  // Dismiss the overlay without scrolling in between.
  SetAdDisplayed(ad, false);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
  EXPECT_TRUE(ad->IsOverlayPopupAd());
  EXPECT_TRUE(ad->IsPrestitialAd());
}

// A pop-up ad appearing more than `kPrestitialWindow` after the first
// contentful paint is not a prestitial: the element is tagged as a pop-up ad
// only.
TEST_F(OverlayInterstitialAdDetectorTest,
       NoPrestitialAdOutsideWindowAfterFirstContentfulPaint) {
  Element* ad =
      SetUpPageWithOverlayAd(/*scrollable=*/false, /*ad_hidden=*/false);
  SimulateFirstContentfulPaintAt(
      base::TimeTicks::Now() -
      OverlayInterstitialAdDetector::kPrestitialWindow - base::Seconds(1));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_FALSE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
  EXPECT_TRUE(ad->IsOverlayPopupAd());
  EXPECT_FALSE(ad->IsPrestitialAd());
}

// A pop-up ad appearing after the user has interacted with the page is not a
// prestitial, even once the transient activation that would have disqualified
// it as a pop-up altogether has expired.
TEST_F(OverlayInterstitialAdDetectorTest,
       NoPrestitialAdAfterStickyUserActivation) {
  SetUpPageWithOverlayAd(/*scrollable=*/false, /*ad_hidden=*/false);
  SimulateFirstContentfulPaint();

  // Sticky activation only: transient activation would make the overlay
  // unqualified as a pop-up in the first place.
  GetDocument().GetFrame()->SetStickyUserActivationState();
  ASSERT_FALSE(
      LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_FALSE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
}

// A pop-up ad appearing after the user has scrolled the main frame is not a
// prestitial.
TEST_F(OverlayInterstitialAdDetectorTest, NoPrestitialAdAfterUserScroll) {
  Element* ad = SetUpPageWithOverlayAd(/*scrollable=*/true, /*ad_hidden=*/true);
  SimulateFirstContentfulPaint();
  UpdateAllLifecyclePhasesForTest();

  SimulateUserScrollTo(100);
  SetAdDisplayed(ad, true);
  UpdateAllLifecyclePhasesForTest();

  // Dismiss the overlay. The scroll position hasn't changed since the
  // overlay's appearance, so it's still a pop-up.
  SetAdDisplayed(ad, false);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_FALSE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
}

// A scroll position away from the top doesn't by itself mean that the user has
// scrolled: fragment navigation, scroll restoration or scrollTo() move it
// without any user input, so a pop-up ad appearing then is still a prestitial.
TEST_F(OverlayInterstitialAdDetectorTest, PrestitialAdAfterProgrammaticScroll) {
  Element* ad = SetUpPageWithOverlayAd(/*scrollable=*/true, /*ad_hidden=*/true);
  SimulateFirstContentfulPaint();
  UpdateAllLifecyclePhasesForTest();

  ScrollMainFrameProgrammaticallyTo(100);
  SetAdDisplayed(ad, true);
  UpdateAllLifecyclePhasesForTest();

  SetAdDisplayed(ad, false);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
}

// A page that was hidden before the pop-up ad appeared is excluded from the
// prestitial classification.
TEST_F(OverlayInterstitialAdDetectorTest,
       NoPrestitialAdWhenPageWasHiddenBeforeAppearance) {
  SetUpPageWithOverlayAd(/*scrollable=*/false, /*ad_hidden=*/false);
  SimulateFirstContentfulPaint();

  SetPageVisibility(mojom::blink::PageVisibilityState::kHidden);
  SetPageVisibility(mojom::blink::PageVisibilityState::kVisible);

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_FALSE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
}

// The prestitial conditions are evaluated when the pop-up ad appears, so a
// page that is only hidden afterwards, while the pop-up ad is showing, still
// counts: the overlay met every condition while the page was visible.
TEST_F(OverlayInterstitialAdDetectorTest,
       PrestitialAdWhenPageWasHiddenWhileShowing) {
  Element* ad =
      SetUpPageWithOverlayAd(/*scrollable=*/true, /*ad_hidden=*/false);
  SimulateFirstContentfulPaint();

  // The overlay becomes a candidate while the page is visible.
  UpdateAllLifecyclePhasesForTest();

  SetPageVisibility(mojom::blink::PageVisibilityState::kHidden);
  SetPageVisibility(mojom::blink::PageVisibilityState::kVisible);

  SetAdDisplayed(ad, false);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPopupAd));
  EXPECT_TRUE(IsUseCounted(WebFeature::kOverlayPrestitialAd));
}

}  // namespace blink
