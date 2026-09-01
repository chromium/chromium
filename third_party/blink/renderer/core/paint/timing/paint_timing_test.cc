// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"

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
    // Clear any remaining expectations.
    VerifyAndClearExpectations();
    // Unregister the client so we don't get notifications after this, e.g. for
    // images being removed.
    GetPaintTiming().RemoveClient(mock_paint_timing_client_.Get());

    PaintTimingTestBase::TearDown();
  }

 protected:
  MockPaintTimingClient& Client() { return *mock_paint_timing_client_.Get(); }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(mock_paint_timing_client_.Get());
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

}  // namespace blink
