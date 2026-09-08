// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

using testing::_;
using testing::A;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::InSequence;
using testing::IsEmpty;
using testing::Mock;
using testing::NiceMock;
using testing::Ref;
using testing::StrictMock;

namespace blink {
namespace {

MATCHER_P(ForNode, node, "") {
  return arg && arg->GetNode() == node;
}

MATCHER_P(WithPresentationTime, timestamp, "") {
  return arg && arg->PaintTime() == timestamp;
}

class MockPaintTimingClient : public GarbageCollected<MockPaintTimingClient>,
                              public PaintTimingClient {
 public:
  MockPaintTimingClient() {
    // Set things up to ensure tests get all lifecycle events.
    ON_CALL(*this, OnElementLastContentfulPaint(A<TextRecord*>(), _))
        .WillByDefault([](TextRecord* record, bool was_previously_reported) {
          record->SetIsNeededForLargestContentfulPaint(true);
        });
    ON_CALL(*this, OnElementLastContentfulPaint(A<ImageRecord*>()))
        .WillByDefault([](ImageRecord* record) {
          record->SetIsNeededForLargestContentfulPaint(true);
        });
  }

  ~MockPaintTimingClient() override = default;

  MOCK_METHOD(void, OnElementFirstContentfulPaint, (ImageRecord*), (override));
  MOCK_METHOD(void, OnElementLastContentfulPaint, (ImageRecord*), (override));
  MOCK_METHOD(void,
              OnElementLastContentfulPaint,
              (TextRecord*, bool),
              (override));
  MOCK_METHOD(void,
              OnImageRemoved,
              (const LayoutObject&, const MediaTiming*),
              (override));
  MOCK_METHOD(void, OnPaintFinished, (), (override));
  MOCK_METHOD(void,
              OnFramePresented,
              (const HeapVector<Member<ImageRecord>>&,
               const HeapVector<Member<TextRecord>>&,
               const HeapVector<Member<ElementTimingInfo>>&,
               const DOMPaintTimingInfo&),
              (override));
  MOCK_METHOD(void, OnInputOrScroll, (), (override));

  void Trace(Visitor*) const override {}
};

}  // namespace

class PaintTimingTest : public PaintTimingTestBase {
 public:
  void SetUp() override {
    PaintTimingTestBase::SetUp();

    mock_paint_timing_client_ =
        MakeGarbageCollected<StrictMock<MockPaintTimingClient>>();
    GetPaintTiming().AddClient(mock_paint_timing_client_.Get());
  }

  void TearDown() override {
    DetachMockClient();
    PaintTimingTestBase::TearDown();
  }

 protected:
  MockPaintTimingClient& Client() { return *mock_paint_timing_client_.Get(); }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(mock_paint_timing_client_.Get());
  }

  void DetachMockClient() {
    if (!mock_paint_timing_client_) {
      return;
    }

    // Clear any remaining expectations.
    VerifyAndClearExpectations();
    // Unregister the client so we don't get notifications after this, e.g. for
    // images being removed.
    GetPaintTiming().RemoveClient(mock_paint_timing_client_.Get());

    mock_paint_timing_client_.Clear();
  }

 private:
  Persistent<MockPaintTimingClient> mock_paint_timing_client_;
};

TEST_F(PaintTimingTest, PaintTimingClientTextRenderingCallbacks) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target">Text</div>
  )HTML");

  Node* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Render the <div>.
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(target), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Present the frame.
  EXPECT_CALL(
      Client(),
      OnFramePresented(IsEmpty(), ElementsAre(ForNode(target)), IsEmpty(), _));
  SimulatePresentationTime();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, PaintTimingClientTextRepaint) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target">Text</div>
  )HTML");

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Initial rendering.
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(target), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    EXPECT_CALL(Client(),
                OnFramePresented(IsEmpty(), ElementsAre(ForNode(target)),
                                 IsEmpty(), _));
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }

  // Cause the text to be repainted without notifying PaintTiming, which should
  // not trigger callbacks for `target`. This works because there is no
  // associated SoftNavigationContext.
  To<HTMLElement>(target)->setInnerText("TextText");
  // There are no new entries, so OnPaintFinished() is the only callback that
  // should run.
  EXPECT_CALL(Client(), OnPaintFinished());
  SimulateRenderingAndPresentationTime();
  VerifyAndClearExpectations();

  // Cause the text to be repainted, this time with notifying PaintTiming. This
  // should trigger callbacks for `target`.
  To<HTMLElement>(target)->setInnerText("TextTextText");
  GetPaintTimingDetector()
      .GetTextPaintTimingDetector()
      .ResetPaintTrackingOnInteraction(*target->GetLayoutObject());
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(ForNode(target),
                                             /*was_previously_reported=*/true));
    EXPECT_CALL(Client(), OnPaintFinished());
    EXPECT_CALL(Client(),
                OnFramePresented(IsEmpty(), ElementsAre(ForNode(target)),
                                 IsEmpty(), _));
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }
}

TEST_F(PaintTimingTest, PaintTimingClientDelayedPresentationFeedback_Text) {
  SetMainFrameBodyContent(R"HTML(
    <div id="node1">Text</div>
    <div id="node2"></div>
  )HTML");

  Element* node1 = GetElementById("node1");
  ASSERT_TRUE(node1);
  Element* node2 = GetElementById("node2");
  ASSERT_TRUE(node2);

  // Frame 1: paint node1 but don't present yet.
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(node1), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Frame 2: paint node2 but don't present yet.
  To<HTMLElement>(node2)->setInnerText("TextTextText");
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(node2), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Present frame 1.
  EXPECT_CALL(Client(), OnFramePresented(IsEmpty(), ElementsAre(ForNode(node1)),
                                         IsEmpty(), _));
  SimulatePresentationTime();
  VerifyAndClearExpectations();

  // Present frame 2.
  EXPECT_CALL(Client(), OnFramePresented(IsEmpty(), ElementsAre(ForNode(node2)),
                                         IsEmpty(), _));
  SimulatePresentationTime();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, PaintTimingClientImageRenderingCallbacks) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target" style="width:100px;height=100px;"></img>
  )HTML");
  SetImageContent("target", 100, 100);

  Node* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Render the <img>. We should get callbacks for the first and last paints
  // because the image is fully loaded.
  {
    InSequence s;
    EXPECT_CALL(Client(), OnElementFirstContentfulPaint(ForNode(target)));
    EXPECT_CALL(Client(), OnElementLastContentfulPaint(ForNode(target)));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Present the frame.
  EXPECT_CALL(Client(), OnFramePresented(ElementsAre(ForNode(target)),
                                         IsEmpty(), IsEmpty(), _));
  SimulatePresentationTime();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, PaintTimingClientPendingImageCallbacks) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target" width=100 height=300 />
  )HTML");
  SetImageContent("target", 100, 100, /*bytes=*/0, ImageStatus::kPending);

  Node* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Render the <img>. We should only get the first paint callback since the
  // image is pending.
  {
    InSequence s;
    EXPECT_CALL(Client(), OnElementFirstContentfulPaint(ForNode(target)));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Present the frame. We shouldn't get any callbacks because no sufficiently
  // loaded images or text was presented this frame.
  SimulatePresentationTime();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, PaintTimingClientDelayedPresentationFeedback_Image) {
  // TODO(crbug.com/466437443): The divs are necessary here to make the images
  // since otherwise the (union of the) spaces between images count as text and
  // will be considered an LCP candidate (aggregated to <body>). This can be if
  // we filter out whitespace-only text paints.
  SetMainFrameBodyContent(R"HTML(
    <div><img id="img1" width=50 height=50 /></div>
    <div><img id="img2" width=100 height=100 /></div>
  )HTML");

  Element* img1 = GetElementById("img1");
  ASSERT_TRUE(img1);
  Element* img2 = GetElementById("img2");
  ASSERT_TRUE(img2);

  // Frame 1: paint img1 but don't present yet.
  SetImageContent("img1", 50, 50);
  {
    InSequence s;
    EXPECT_CALL(Client(), OnElementFirstContentfulPaint(ForNode(img1)));
    EXPECT_CALL(Client(), OnElementLastContentfulPaint(ForNode(img1)));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Frame 2: paint img2 but don't present yet.
  SetImageContent("img2", 50, 50);
  {
    InSequence s;
    EXPECT_CALL(Client(), OnElementFirstContentfulPaint(ForNode(img2)));
    EXPECT_CALL(Client(), OnElementLastContentfulPaint(ForNode(img2)));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Present frame 1.
  EXPECT_CALL(Client(), OnFramePresented(ElementsAre(ForNode(img1)), IsEmpty(),
                                         IsEmpty(), _));
  SimulatePresentationTime();
  VerifyAndClearExpectations();

  // Present frame 2.
  EXPECT_CALL(Client(), OnFramePresented(ElementsAre(ForNode(img2)), IsEmpty(),
                                         IsEmpty(), _));
  SimulatePresentationTime();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, PendingImageRemoval) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target" width=100 height=100 />
  )HTML");
  MediaTiming* timing =
      SetImageContent("target", 100, 100, /*bytes*/ 0, ImageStatus::kPending);

  Node* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Render and present the <img>. We should only get the first paint callback
  // since the image is pending.
  {
    InSequence s;
    EXPECT_CALL(Client(), OnElementFirstContentfulPaint(ForNode(target)));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }

  // Remove the image. This should cause the client to be notified with the
  // pending `ImageRecord`, along with the `LayoutObject` and `MediaTiming`.
  const LayoutObject* object = target->GetLayoutObject();
  ASSERT_TRUE(object);
  EXPECT_CALL(Client(), OnImageRemoved(Ref(*object), Eq(timing)));
  target->remove();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, LoadedImageRemoval) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target" width=100 height=100 />
  )HTML");
  MediaTiming* timing = SetImageContent("target", 100, 100);

  Node* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Render and present the <img>. We should only get the first paint callback
  // since the image is pending.
  {
    InSequence s;
    EXPECT_CALL(Client(), OnElementFirstContentfulPaint(ForNode(target)));
    EXPECT_CALL(Client(), OnElementLastContentfulPaint(ForNode(target)));
    EXPECT_CALL(Client(), OnPaintFinished());
    EXPECT_CALL(Client(), OnFramePresented(ElementsAre(ForNode(target)),
                                           IsEmpty(), IsEmpty(), _));
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }

  // Remove the image. This should cause the client to be notified with the with
  // the `LayoutObject` and `MediaTiming`, but not the loaded `ImageRecord`.
  const LayoutObject* object = target->GetLayoutObject();
  ASSERT_TRUE(object);
  EXPECT_CALL(Client(), OnImageRemoved(Ref(*object), Eq(timing)));
  target->remove();
  VerifyAndClearExpectations();
}

TEST_F(PaintTimingTest, DiscreteInput) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target">Text</div>
  )HTML");

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Initial rendering.
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(target), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    EXPECT_CALL(Client(),
                OnFramePresented(IsEmpty(), ElementsAre(ForNode(target)),
                                 IsEmpty(), _));
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }
  EXPECT_NE(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);

  // Simulate input.
  EXPECT_CALL(Client(), OnInputOrScroll());
  SimulateKeyDown();
  VerifyAndClearExpectations();
  EXPECT_EQ(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);

  // Simulate a second input.
  EXPECT_CALL(Client(), OnInputOrScroll());
  SimulateKeyDown();
  VerifyAndClearExpectations();
  EXPECT_EQ(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);
}

TEST_F(PaintTimingTest, UserInitiatedScroll) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target">Text</div>
  )HTML");

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Initial rendering.
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(target), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    EXPECT_CALL(Client(),
                OnFramePresented(IsEmpty(), ElementsAre(ForNode(target)),
                                 IsEmpty(), _));
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }
  EXPECT_NE(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);

  // Simulate a user-initated scroll.
  EXPECT_CALL(Client(), OnInputOrScroll());
  SimulateScroll(mojom::blink::ScrollType::kUser);
  VerifyAndClearExpectations();
  EXPECT_EQ(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);

  // Simulate a second scroll.
  EXPECT_CALL(Client(), OnInputOrScroll());
  SimulateScroll(mojom::blink::ScrollType::kUser);
  VerifyAndClearExpectations();
  EXPECT_EQ(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);
}

TEST_F(PaintTimingTest, ProgrammaticScroll) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target">Text</div>
  )HTML");

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);

  // Initial rendering.
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(target), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    EXPECT_CALL(Client(),
                OnFramePresented(IsEmpty(), ElementsAre(ForNode(target)),
                                 IsEmpty(), _));
    SimulateRenderingAndPresentationTime();
    VerifyAndClearExpectations();
  }
  EXPECT_NE(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);

  // Simulate a programmatic scroll. Clients will not be notified for this.
  SimulateScroll(mojom::blink::ScrollType::kProgrammatic);
  VerifyAndClearExpectations();
  EXPECT_NE(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);

  // Simulate a second scroll.
  SimulateScroll(mojom::blink::ScrollType::kProgrammatic);
  VerifyAndClearExpectations();
  EXPECT_NE(GetPaintTiming().GetLargestContentfulPaintManager(), nullptr);
}

class PaintTimingOutOfOrderPresentationTimeTest
    : public PaintTimingTest,
      public testing::WithParamInterface<bool> {
 public:
  PaintTimingOutOfOrderPresentationTimeTest() {
    if (IsWaitForPresentationFrameIndexEnabled()) {
      feature_list_.InitWithFeatures(
          {kPaintTimingWaitForPresentationFrameIndex}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {kPaintTimingWaitForPresentationFrameIndex});
    }
  }

  void SetPresentationTime() {
    AdvanceClock(base::Milliseconds(100));
    GetMockPaintTimingCallbackManager()->OnAnimationFramePresented(
        base::TimeTicks::Now());
  }

  void InvokeLastPresentationCallback() {
    GetMockPaintTimingCallbackManager()->InvokeCallbacksForLastAnimationFrame();
  }

  bool IsWaitForPresentationFrameIndexEnabled() const { return GetParam(); }

  Element* AppendDivElementToBody(String content) {
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    Text* text = GetDocument().createTextNode(content);
    div->AppendChild(text);
    GetDocument().body()->AppendChild(div);
    return div;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

namespace {

constexpr char kPresentationCallbackIdDeltaMetricName[] =
    "Renderer.PaintTiming.PresentationCallbackIdDelta";

constexpr char kPresentationTimeDeltaMetricName[] =
    "Renderer.PaintTiming.PresentationTimeDelta";

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         PaintTimingOutOfOrderPresentationTimeTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "WaitForPresentationFrameIndexEnabled"
                                      : "WaitForPresentationFrameIndexDisabled";
                         });

TEST_P(PaintTimingOutOfOrderPresentationTimeTest, CallbackOrder) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target1">Text</div>
  )HTML");
  // Frame 1: render the initial text.
  Element* div1 = GetElementById("target1");
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(div1), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Frame 2: Append and render more text.
  Element* div2 = AppendDivElementToBody("Text Text");
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(div2), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Frame 3: Append and render more text.
  Element* div3 = AppendDivElementToBody("Text Text Text");
  {
    InSequence s;
    EXPECT_CALL(Client(),
                OnElementLastContentfulPaint(
                    ForNode(div3), /*was_previously_reported=*/false));
    EXPECT_CALL(Client(), OnPaintFinished());
    SimulateRendering();
    VerifyAndClearExpectations();
  }

  // Set presentation time for frame 1.
  SetPresentationTime();
  base::TimeTicks timestamp1 = base::TimeTicks::Now();

  // Set presentation time for frame 2.
  SetPresentationTime();
  base::TimeTicks timestamp2 = base::TimeTicks::Now();
  EXPECT_NE(timestamp1, timestamp2);

  // Set presentation time for frame 3.
  SetPresentationTime();
  base::TimeTicks timestamp3 = base::TimeTicks::Now();
  EXPECT_NE(timestamp2, timestamp3);

  // Without kPaintTimingWaitForPresentationFrameIndex enabled, all of the
  // timestamps should match `timestamp3`. Overwrite them here to make the
  // expectations below more readable.
  if (!IsWaitForPresentationFrameIndexEnabled()) {
    timestamp1 = timestamp3;
    timestamp2 = timestamp3;
  }

  // Invoke callbacks in reverse order.
  InSequence s;

  EXPECT_CALL(
      Client(),
      OnFramePresented(
          IsEmpty(),
          ElementsAre(AllOf(ForNode(div1), WithPresentationTime(timestamp1))),
          IsEmpty(), _));
  EXPECT_CALL(
      Client(),
      OnFramePresented(
          IsEmpty(),
          ElementsAre(AllOf(ForNode(div2), WithPresentationTime(timestamp2))),
          IsEmpty(), _));
  EXPECT_CALL(
      Client(),
      OnFramePresented(
          IsEmpty(),
          ElementsAre(AllOf(ForNode(div3), WithPresentationTime(timestamp3))),
          IsEmpty(), _));
  InvokeLastPresentationCallback();
  InvokeLastPresentationCallback();
  InvokeLastPresentationCallback();
  VerifyAndClearExpectations();
}

TEST_P(PaintTimingOutOfOrderPresentationTimeTest, TestInOrderHistogram) {
  // Histograms are only logged with the feature enabled.
  if (!IsWaitForPresentationFrameIndexEnabled()) {
    return;
  }
  DetachMockClient();
  base::HistogramTester histogram_tester;

  SetMainFrameBodyContent(R"HTML(
    <div id="target1">Text</div>
  )HTML");
  // Frame 1: render the initial text.
  SimulateRendering();

  // Frame 2: Append and render more text.
  AppendDivElementToBody("Text Text");
  SimulateRendering();

  // Frame 3: Append and render more text.
  AppendDivElementToBody("Text Text Text");
  SimulateRendering();

  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 0);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 0);

  SimulatePresentationTime();
  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 1);
  histogram_tester.ExpectBucketCount(kPresentationCallbackIdDeltaMetricName, 0,
                                     1);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 1);
  histogram_tester.ExpectTimeBucketCount(kPresentationTimeDeltaMetricName,
                                         base::TimeDelta(), 1);

  SimulatePresentationTime();
  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 2);
  histogram_tester.ExpectBucketCount(kPresentationCallbackIdDeltaMetricName, 0,
                                     2);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 2);
  histogram_tester.ExpectTimeBucketCount(kPresentationTimeDeltaMetricName,
                                         base::TimeDelta(), 2);

  SimulatePresentationTime();
  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 3);
  histogram_tester.ExpectBucketCount(kPresentationCallbackIdDeltaMetricName, 0,
                                     3);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 3);
  histogram_tester.ExpectTimeBucketCount(kPresentationTimeDeltaMetricName,
                                         base::TimeDelta(), 3);
}

TEST_P(PaintTimingOutOfOrderPresentationTimeTest, TestOutOfOrderHistogram) {
  // Histograms are only logged with the feature enabled.
  if (!IsWaitForPresentationFrameIndexEnabled()) {
    return;
  }
  DetachMockClient();
  base::HistogramTester histogram_tester;

  SetMainFrameBodyContent(R"HTML(
    <div id="target1">Text</div>
  )HTML");
  // Frame 1: render the initial text.
  SimulateRendering();

  // Frame 2: Append and render more text.
  AppendDivElementToBody("Text Text");
  SimulateRendering();

  // Frame 3: Append and render more text.
  AppendDivElementToBody("Text Text Text");
  SimulateRendering();

  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 0);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 0);

  // Set the presentation time for all three frames. The time delta metric isn't
  // log until the callback data is processed.
  SetPresentationTime();
  base::TimeTicks presentation_time1 = base::TimeTicks::Now();

  SetPresentationTime();
  base::TimeTicks presentation_time2 = base::TimeTicks::Now();

  SetPresentationTime();
  base::TimeTicks presentation_time3 = base::TimeTicks::Now();

  // Execute the presentation callbacks in reverse order. There should be one
  // entry for each of three buckets: 0 (underflow), 1, and 2.
  InvokeLastPresentationCallback();
  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 1);
  histogram_tester.ExpectBucketCount(kPresentationCallbackIdDeltaMetricName, 2,
                                     1);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 0);

  InvokeLastPresentationCallback();
  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 2);
  histogram_tester.ExpectBucketCount(kPresentationCallbackIdDeltaMetricName, 1,
                                     1);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 0);

  InvokeLastPresentationCallback();
  histogram_tester.ExpectTotalCount(kPresentationCallbackIdDeltaMetricName, 3);
  histogram_tester.ExpectBucketCount(kPresentationCallbackIdDeltaMetricName, 0,
                                     1);
  histogram_tester.ExpectTotalCount(kPresentationTimeDeltaMetricName, 3);

  // Frame 3:
  histogram_tester.ExpectTimeBucketCount(kPresentationTimeDeltaMetricName,
                                         base::TimeDelta(), 1);
  // Frame 2:
  histogram_tester.ExpectTimeBucketCount(
      kPresentationTimeDeltaMetricName, presentation_time3 - presentation_time2,
      1);
  // Frame 1:
  histogram_tester.ExpectTimeBucketCount(
      kPresentationTimeDeltaMetricName, presentation_time3 - presentation_time1,
      1);
}

}  // namespace blink
