// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"

#include <gtest/gtest.h>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_scrollintoviewoptions.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_test_utils.h"
#include "third_party/blink/renderer/core/annotation/node_annotation_selector.h"
#include "third_party/blink/renderer/core/annotation/text_annotation_selector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/glic_marker.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class AnnotationAgentImplTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

 protected:
  AnnotationAgentImplTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {
            ::features::kProgrammaticScrollAnimationOverride,
            {
                {"cubic_bezier_x1", "0.4"},          //
                {"cubic_bezier_y1", "0.0"},          //
                {"cubic_bezier_x2", "0.0"},          //
                {"cubic_bezier_y2", "1.0"},          //
                {"max_animation_duration", "1.5s"},  //
            }  //
        }  //
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        std::move(enabled_features),
        /*disabled_features=*/{});
  }

  // Helper to create a range to some text within a single text node. Verifies
  // the Range selects the `expected` text.
  RangeInFlatTree* CreateRangeToExpectedText(Node* node,
                                             int start_offset,
                                             int end_offset,
                                             const String& expected) {
    EXPECT_TRUE(node);
    if (!node) {
      return nullptr;
    }

    const auto& range_start = Position(node, start_offset);
    const auto& range_end = Position(node, end_offset);

    String actual = PlainText(EphemeralRange(range_start, range_end));
    EXPECT_EQ(expected, actual);
    if (expected != actual)
      return nullptr;

    return MakeGarbageCollected<RangeInFlatTree>(
        ToPositionInFlatTree(range_start), ToPositionInFlatTree(range_end));
  }

  RangeInFlatTree* CreateRangeForWholeDocument(Document& document) {
    const auto& range_start = PositionInFlatTree::FirstPositionInNode(document);
    const auto& range_end = PositionInFlatTree::LastPositionInNode(document);
    return MakeGarbageCollected<RangeInFlatTree>(
        ToPositionInFlatTree(range_start), ToPositionInFlatTree(range_end));
  }

  // Returns the number of annotation markers that intersect the given range.
  wtf_size_t NumMarkersInRange(RangeInFlatTree& range) {
    return GetDocument()
        .Markers()
        .MarkersIntersectingRange(range.ToEphemeralRange(),
                                  DocumentMarker::MarkerTypes::TextFragment())
        .size();
  }

  // Creates an agent with a mock selector that will relect the given range
  // when attached.
  AnnotationAgentImpl* CreateAgentForRange(
      RangeInFlatTree* range,
      mojom::blink::AnnotationType type =
          mojom::blink::AnnotationType::kSharedHighlight) {
    EXPECT_TRUE(range);
    if (!range)
      return nullptr;

    auto* container =
        AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
    EXPECT_TRUE(container);
    if (!container)
      return nullptr;

    auto* mock_selector = MakeGarbageCollected<MockAnnotationSelector>(*range);
    return container->CreateUnboundAgent(type, *mock_selector);
  }

  // Creates an agent with a mock selector that will always fail to find a
  // range when attaching.
  AnnotationAgentImpl* CreateAgentFailsAttach() {
    auto* container =
        AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
    EXPECT_TRUE(container);
    if (!container)
      return nullptr;

    auto* null_range = MakeGarbageCollected<RangeInFlatTree>();
    DCHECK(null_range->IsNull());

    auto* mock_selector =
        MakeGarbageCollected<MockAnnotationSelector>(*null_range);
    return container->CreateUnboundAgent(
        mojom::blink::AnnotationType::kSharedHighlight, *mock_selector);
  }

  // Creates an agent with a real text selector that will perform a real search
  // of the DOM tree. Use a text selector string of the same format as a URL's
  // text directive. (i.e. the part that comes after ":~:text=")
  AnnotationAgentImpl* CreateTextFinderAgent(
      const String& text_selector,
      mojom::blink::AnnotationType type =
          mojom::blink::AnnotationType::kSharedHighlight) {
    auto* selector = MakeGarbageCollected<TextAnnotationSelector>(
        TextFragmentSelector::FromTextDirective(text_selector));
    auto* container =
        AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
    return container->CreateUnboundAgent(type, *selector);
  }

  AnnotationAgentImpl* CreateNodeAgent(
      DOMNodeId dom_node_id,
      mojom::blink::AnnotationType type = mojom::blink::AnnotationType::kGlic) {
    auto* selector = MakeGarbageCollected<NodeAnnotationSelector>(dom_node_id);
    auto* container =
        AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
    return container->CreateUnboundAgent(type, *selector);
  }

  // Performs a check that the given node is fully visible in the visual
  // viewport - that is, it's entire bounding rect is contained in the visual
  // viewport. Returns whether the check passed so it can be used as an ASSERT
  // in tests.
  bool ExpectInViewport(Node& node) {
    VisualViewport& viewport =
        GetDocument().View()->GetPage()->GetVisualViewport();
    gfx::Rect rect_in_visual_viewport = viewport.RootFrameToViewport(
        node.GetLayoutObject()->AbsoluteBoundingBoxRect(
            kTraverseDocumentBoundaries));
    gfx::Rect viewport_rect(viewport.Size());

    bool is_contained = viewport_rect.Contains(rect_in_visual_viewport);
    EXPECT_TRUE(is_contained)
        << "Expected [" << node.DebugName()
        << "] to be visible in viewport. Bounds relative to viewport: ["
        << rect_in_visual_viewport.ToString() << "] vs. viewport bounds [ "
        << viewport_rect.ToString() << " ]";
    return is_contained;
  }

  // Opposite of above. Duplicated to provide correct error message when
  // expectation fails.
  bool ExpectNotInViewport(Node& node) {
    VisualViewport& viewport =
        GetDocument().View()->GetPage()->GetVisualViewport();
    gfx::Rect rect_in_visual_viewport = viewport.RootFrameToViewport(
        node.GetLayoutObject()->AbsoluteBoundingBoxRect(
            kTraverseDocumentBoundaries));
    gfx::Rect viewport_rect(viewport.Size());

    bool is_contained = viewport_rect.Contains(rect_in_visual_viewport);
    EXPECT_FALSE(is_contained)
        << "Expected [" << node.DebugName()
        << "] to be visible in viewport. Bounds relative to viewport: ["
        << rect_in_visual_viewport.ToString() << "] vs. viewport bounds [ "
        << viewport_rect.ToString() << " ]";
    return !is_contained;
  }

  mojom::blink::AnnotationType GetAgentType(AnnotationAgentImpl* agent) {
    return agent->type_;
  }

  bool IsRemoved(AnnotationAgentImpl* agent) { return agent->IsRemoved(); }

  void LoadAhem() {
    std::optional<Vector<char>> data =
        test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
    ASSERT_TRUE(data);
    auto* buffer =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            DOMArrayBuffer::Create(base::as_byte_span(*data)));
    FontFace* ahem = FontFace::Create(GetDocument().GetFrame()->DomWindow(),
                                      AtomicString("Ahem"), buffer,
                                      FontFaceDescriptors::Create());

    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    DummyExceptionStateForTesting exception_state;
    FontFaceSetDocument::From(GetDocument())
        ->addForBinding(script_state, ahem, exception_state);
  }

  DocumentMarkerVector GetAllMarkers() {
    return GetDocument().Markers().Markers();
  }

  float GetAlphaForGlicMarkerAt(size_t index) {
    const auto& markers = GetAllMarkers();
    EXPECT_GE(markers.size(), index + 1);
    DocumentMarker* marker = markers[index];
    EXPECT_EQ(marker->GetType(), DocumentMarker::MarkerType::kGlic);
    return To<GlicMarker>(marker)->BackgroundColor().Alpha();
  }

  bool GlicAnimationRunning() {
    return GetDocument().Markers().glic_animation_state_ ==
           DocumentMarkerController::GlicAnimationState::kRunning;
  }

  bool GlicAnimationFinished() {
    return GetDocument().Markers().glic_animation_state_ ==
           DocumentMarkerController::GlicAnimationState::kFinished;
  }

  bool GlicAnimationNotStarted() {
    return GetDocument().Markers().glic_animation_state_ ==
           DocumentMarkerController::GlicAnimationState::kNotStarted;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the agent type is correctly set.
TEST_F(AnnotationAgentImplTest, AgentType) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* shared_highlight_agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  auto* user_note_agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kUserNote,
      *MakeGarbageCollected<MockAnnotationSelector>());

  auto* glic_highlight_agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kGlic,
      *MakeGarbageCollected<MockAnnotationSelector>());

  EXPECT_EQ(GetAgentType(shared_highlight_agent),
            mojom::blink::AnnotationType::kSharedHighlight);
  EXPECT_EQ(GetAgentType(user_note_agent),
            mojom::blink::AnnotationType::kUserNote);
  EXPECT_EQ(GetAgentType(glic_highlight_agent),
            mojom::blink::AnnotationType::kGlic);
}

// Ensure that simply creating an (unbound) agent doesn't automatically try to
// attach to DOM or bind to a mojo endpoint.
TEST_F(AnnotationAgentImplTest, CreatingDoesntBindOrAttach) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  EXPECT_FALSE(agent->IsBoundForTesting());
  EXPECT_FALSE(agent->IsAttached());
}

// Tests that binding works.
TEST_F(AnnotationAgentImplTest, Bind) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);

  EXPECT_TRUE(host.agent_.is_connected());
  EXPECT_FALSE(host.did_disconnect_);
}

// Tests that removing the agent disconnects bindings.
TEST_F(AnnotationAgentImplTest, RemoveDisconnectsBindings) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);

  ASSERT_TRUE(host.agent_.is_connected());
  ASSERT_FALSE(host.did_disconnect_);

  container->RemoveAgent(*agent);
  host.FlushForTesting();

  EXPECT_FALSE(host.agent_.is_connected());
  EXPECT_TRUE(host.did_disconnect_);
}

// Tests that removing an agent clears all its state.
TEST_F(AnnotationAgentImplTest, RemoveClearsState) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  EXPECT_FALSE(IsRemoved(agent));

  Compositor().BeginFrame();
  ASSERT_TRUE(agent->IsAttached());

  container->RemoveAgent(*agent);

  EXPECT_TRUE(IsRemoved(agent));
  EXPECT_FALSE(agent->IsAttached());
}

// Tests that attaching an agent to DOM in the document happens in a BeginFrame.
TEST_F(AnnotationAgentImplTest, AttachDuringBeginFrame) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  ASSERT_FALSE(agent->IsAttached());
  Compositor().BeginFrame();
  EXPECT_TRUE(agent->IsAttached());
}

// Tests that attaching an agent to DOM will cause a document marker to be
// placed at the attached Range.
TEST_F(AnnotationAgentImplTest, SuccessfulAttachCreatesMarker) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  Node* p = GetDocument().getElementById(AtomicString("text"))->firstChild();

  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 5, 8, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar = CreateRangeToExpectedText(p, 14, 17, "BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  Compositor().BeginFrame();

  ASSERT_TRUE(agent_foo->IsAttached());
  ASSERT_TRUE(agent_bar->IsAttached());

  // Both "FOO" and "BAR" should each have a single marker.
  EXPECT_EQ(NumMarkersInRange(*range_foo), 1ul);
  EXPECT_EQ(NumMarkersInRange(*range_bar), 1ul);

  // Ensure we didn't create markers outside of the selected text.
  RangeInFlatTree* range_test = CreateRangeToExpectedText(p, 0, 4, "TEST");
  RangeInFlatTree* range_page = CreateRangeToExpectedText(p, 9, 13, "PAGE");
  EXPECT_EQ(NumMarkersInRange(*range_test), 0ul);
  EXPECT_EQ(NumMarkersInRange(*range_page), 0ul);
}

// Tests that removing an agent will cause its corresponding document marker to
// be removed as well.
TEST_F(AnnotationAgentImplTest, RemovedAgentRemovesMarkers) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  Node* p = GetDocument().getElementById(AtomicString("text"))->firstChild();

  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 5, 8, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar = CreateRangeToExpectedText(p, 14, 17, "BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  Compositor().BeginFrame();
  ASSERT_EQ(NumMarkersInRange(*range_foo), 1ul);
  ASSERT_EQ(NumMarkersInRange(*range_bar), 1ul);

  AnnotationAgentContainerImpl* container = agent_foo->OwningContainer();
  container->RemoveAgent(*agent_foo);

  ASSERT_EQ(NumMarkersInRange(*range_foo), 0ul);
  ASSERT_EQ(NumMarkersInRange(*range_bar), 1ul);

  container->RemoveAgent(*agent_bar);

  ASSERT_EQ(NumMarkersInRange(*range_foo), 0ul);
  ASSERT_EQ(NumMarkersInRange(*range_bar), 0ul);
}

// Tests the case where an agent's selector fails to attach to any content in
// the DOM. Ensure markers aren't created but the agent remains in a live
// state.
TEST_F(AnnotationAgentImplTest, AgentFailsAttachment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  auto* agent = CreateAgentFailsAttach();
  ASSERT_TRUE(agent);

  Node* p = GetDocument().getElementById(AtomicString("text"))->firstChild();
  RangeInFlatTree* range =
      CreateRangeToExpectedText(p, 0, 17, "TEST FOO PAGE BAR");
  ASSERT_EQ(NumMarkersInRange(*range), 0ul);

  Compositor().BeginFrame();

  EXPECT_EQ(NumMarkersInRange(*range), 0ul);
  EXPECT_FALSE(agent->IsAttached());
  EXPECT_FALSE(IsRemoved(agent));
}

// Tests that failing to attach is still reported to the host the attempt
// completes.
TEST_F(AnnotationAgentImplTest, AgentFailsAttachmentReportsToHost) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  auto* agent = CreateAgentFailsAttach();
  ASSERT_TRUE(agent);

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);

  ASSERT_FALSE(host.did_disconnect_);
  ASSERT_TRUE(host.agent_.is_connected());
  ASSERT_FALSE(host.did_finish_attachment_rect_);

  Compositor().BeginFrame();
  host.FlushForTesting();

  ASSERT_TRUE(host.did_finish_attachment_rect_);
  EXPECT_TRUE(host.did_finish_attachment_rect_->IsEmpty());
}

// Tests that an overlapping marker still reports a completed attachment to the
// host.
TEST_F(AnnotationAgentImplTest, AttachmentToOverlappingMarkerReportsToHost) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  Node* text_node =
      GetDocument().getElementById(AtomicString("text"))->firstChild();

  auto* agent = CreateAgentFailsAttach();
  ASSERT_TRUE(agent);
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(text_node, 5, 13, "FOO PAGE");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar =
      CreateRangeToExpectedText(text_node, 9, 17, "PAGE BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  MockAnnotationAgentHost host_foo;
  MockAnnotationAgentHost host_bar;
  host_foo.BindToAgent(*agent_foo);
  host_bar.BindToAgent(*agent_bar);

  ASSERT_FALSE(host_foo.did_finish_attachment_rect_);
  ASSERT_FALSE(host_bar.did_finish_attachment_rect_);

  Compositor().BeginFrame();

  ASSERT_TRUE(agent_foo->IsAttached());
  ASSERT_TRUE(agent_bar->IsAttached());

  host_foo.FlushForTesting();

  EXPECT_TRUE(host_foo.did_finish_attachment_rect_);
  EXPECT_TRUE(host_bar.did_finish_attachment_rect_);
}

// Tests that attached agents report the document-coordinate rects of the
// ranges to the host.
TEST_F(AnnotationAgentImplTest, AttachmentReportsRectsToHost) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 1000px;
      }
      #bar {
        position: absolute;
        top: 2000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
    <p id='bar'>BAR</p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  // Zoom and scroll to non-default values so we can ensure the coordinates of
  // the attached ranges are in the document coordinate space.
  {
    WebView().SetPageScaleFactor(2);
    GetDocument().View()->GetRootFrameViewport()->SetScrollOffset(
        ScrollOffset(123, 3000), mojom::blink::ScrollType::kProgrammatic,
        cc::ScrollSourceType::kNone, mojom::blink::ScrollBehavior::kInstant,
        ScrollableArea::ScrollCallback());

    // The visual viewport consumes all the horizontal scroll and 300px (its
    // max scroll offset) of the vertical scroll.
    VisualViewport& viewport =
        GetDocument().View()->GetPage()->GetVisualViewport();
    ASSERT_EQ(viewport.Scale(), 2.f);
    ASSERT_EQ(viewport.GetScrollOffset(), ScrollOffset(123, 300));
    ASSERT_EQ(GetDocument().View()->LayoutViewport()->GetScrollOffset(),
              ScrollOffset(0, 2700));
  }

  Node* node_foo =
      GetDocument().getElementById(AtomicString("foo"))->firstChild();
  Node* node_bar =
      GetDocument().getElementById(AtomicString("bar"))->firstChild();

  RangeInFlatTree* range_foo = CreateRangeToExpectedText(node_foo, 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar = CreateRangeToExpectedText(node_bar, 0, 3, "BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  MockAnnotationAgentHost host_foo;
  MockAnnotationAgentHost host_bar;

  host_foo.BindToAgent(*agent_foo);
  host_bar.BindToAgent(*agent_bar);

  ASSERT_FALSE(host_foo.did_finish_attachment_rect_);
  ASSERT_FALSE(host_bar.did_finish_attachment_rect_);

  Compositor().BeginFrame();

  EXPECT_TRUE(agent_foo->IsAttached());
  EXPECT_TRUE(agent_bar->IsAttached());

  host_foo.FlushForTesting();
  host_bar.FlushForTesting();

  ASSERT_TRUE(host_foo.did_finish_attachment_rect_);
  ASSERT_TRUE(host_bar.did_finish_attachment_rect_);

  EXPECT_EQ(*host_foo.did_finish_attachment_rect_, gfx::Rect(0, 1010, 30, 10));
  EXPECT_EQ(*host_bar.did_finish_attachment_rect_, gfx::Rect(0, 2010, 30, 10));
}

// Tests that calling ScrollIntoView will ensure the marker is in the viewport.
TEST_F(AnnotationAgentImplTest, AgentScrollIntoView) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 1000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

// Tests that calling ScrollIntoView with `applies_focus` set to false will not
// focus the element or clear the focus.
TEST_F(AnnotationAgentImplTest, AgentScrollIntoViewDoesNotApplyFocus) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 1000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo' tabindex=0>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());
  ASSERT_FALSE(element_foo->IsFocusedElementInDocument());
  ASSERT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            nullptr);

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_foo));
  EXPECT_FALSE(element_foo->IsFocusedElementInDocument());
  ASSERT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            element_foo);
}

// Tests that calling ScrollIntoView with `applies_focus` set to true will
// ensure that the element will be focused if focusable.
TEST_F(AnnotationAgentImplTest, AgentScrollIntoViewAppliesFocusAndFocusable) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 1000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo' tabindex=0>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());
  ASSERT_FALSE(element_foo->IsFocusedElementInDocument());
  ASSERT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            nullptr);

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/true);
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_foo));
  EXPECT_TRUE(element_foo->IsFocusedElementInDocument());
  ASSERT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            element_foo);
}

// Tests that calling ScrollIntoView with `applies_focus` set to true will
// ensure that the focus is cleared and the sequential tab navigation point is
// correctly set if the element is not focusable.
TEST_F(AnnotationAgentImplTest,
       AgentScrollIntoViewAppliesFocusAndNotFocusable) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 1000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());
  ASSERT_FALSE(element_foo->IsFocusedElementInDocument());
  ASSERT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            nullptr);

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/true);
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_foo));
  EXPECT_FALSE(element_foo->IsFocusedElementInDocument());
  ASSERT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            element_foo);
}

// Tests that calling ScrollIntoView will ensure the marker is in the viewport
// when the page has been pinch-zoomed.
TEST_F(AnnotationAgentImplTest, AgentScrollIntoViewZoomed) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 5000px;
        left: 400px;
      }
      body {
        margin: 0;
        height: 10000px;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  // The page is non-horizontally scrollable but pinch-zoom so that the "FOO"
  // text is just off-screen on the right. This will ensure ScrollIntoView also
  // moves the visual viewport if the user pinch-zoomed in.
  WebView().SetPageScaleFactor(2);

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

// Test that calling ScrollIntoView while layout is dirty causes the page to
// update layout and correctly ScrollIntoView the agent.
TEST_F(AnnotationAgentImplTest, ScrollIntoViewWithDirtyLayout) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        position: absolute;
        top: 100px;
      }
    </style>
    <p id='text'>FOO BAR</p>
  )HTML");

  Compositor().BeginFrame();

  Element* element_text = GetDocument().getElementById(AtomicString("text"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_text->firstChild(), 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectInViewport(*element_text));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  element_text->setAttribute(html_names::kStyleAttr,
                             AtomicString("top: 2000px"));

  // Invoking ScrollIntoView on the agent should perform layout and then cause
  // the attached content to scroll into the viewport.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_text));
  EXPECT_GT(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset().y(),
            1000);
}

// Degenerate case but make sure it doesn't crash. This constructs a
// RangeInFlatTree that isn't collapsed but turns into a collapsed
// EphmemeralRangeInFlatTree.
TEST_F(AnnotationAgentImplTest, ScrollIntoViewCollapsedRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id='text'>a</p>

  )HTML");

  Compositor().BeginFrame();

  Element* element_text = GetDocument().getElementById(AtomicString("text"));

  const auto& range_start =
      Position(element_text->firstChild(), PositionAnchorType::kBeforeAnchor);
  const auto& range_end = Position(element_text, 0);

  RangeInFlatTree* range = MakeGarbageCollected<RangeInFlatTree>(
      ToPositionInFlatTree(range_start), ToPositionInFlatTree(range_end));

  // TODO(bokan): Is this an editing bug?
  ASSERT_FALSE(range->IsCollapsed());
  ASSERT_TRUE(range->ToEphemeralRange().IsCollapsed());

  auto* agent = CreateAgentForRange(range);
  ASSERT_TRUE(agent);

  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);
  Compositor().BeginFrame();

  // Attachment should fail for this collapsed range.
  EXPECT_FALSE(agent->IsAttached());
  host.FlushForTesting();

  // Ensure calling ScrollIntoView doesn't crash.
  host.agent_->ScrollIntoView(/*applies_focus=*/false);
  host.FlushForTesting();
  EXPECT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset().y(),
            0);
}

// Ensure an annotation causes a hidden <details> section to be opened when
// text inside it is attached.
TEST_F(AnnotationAgentImplTest, OpenDetailsElement) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      details {
        position: absolute;
        top: 2000px;
      }
    </style>
    <details id='details'>foobar</p>
  )HTML");

  Compositor().BeginFrame();

  Element* element_details =
      GetDocument().getElementById(AtomicString("details"));
  ASSERT_FALSE(element_details->FastHasAttribute(html_names::kOpenAttr));

  auto* agent = CreateTextFinderAgent("foobar");
  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);

  EXPECT_FALSE(agent->IsAttachmentPending());
  Compositor().BeginFrame();
  host.FlushForTesting();

  // Since the matching text is inside a <details> it is initially hidden. The
  // attachment will be asynchronous as the <details> element must be opened
  // which needs to happen in a safe place during the document lifecycle.
  EXPECT_TRUE(agent->IsAttachmentPending());
  EXPECT_FALSE(agent->IsAttached());
  EXPECT_FALSE(host.did_finish_attachment_rect_);
  EXPECT_FALSE(element_details->FastHasAttribute(html_names::kOpenAttr));

  // ScrollIntoView, if called, shouldn't cause a scroll yet.
  agent->ScrollIntoView(/*applies_focus=*/false);
  EXPECT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Produce a compositor frame. This should process the DOM mutations and
  // finish attaching the agent.
  Compositor().BeginFrame();
  host.FlushForTesting();

  EXPECT_TRUE(element_details->FastHasAttribute(html_names::kOpenAttr));
  EXPECT_FALSE(agent->IsAttachmentPending());
  EXPECT_TRUE(agent->IsAttached());
  EXPECT_TRUE(host.did_finish_attachment_rect_);

  // ScrollIntoView should now correctly scroll to the expanded details element.
  agent->ScrollIntoView(/*applies_focus=*/false);
  EXPECT_GT(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset().y(),
            100.f);
}

// Ensure an annotation causes a `hidden=until-found` section to be shown when
// text inside it is attached.
TEST_F(AnnotationAgentImplTest, OpenHiddenUntilFoundElement) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="section" hidden="until-found">foobar</p>
  )HTML");

  Compositor().BeginFrame();

  Element* element = GetDocument().getElementById(AtomicString("section"));

  auto* agent = CreateTextFinderAgent("foobar");

  Compositor().BeginFrame();

  EXPECT_TRUE(element->FastHasAttribute(html_names::kHiddenAttr));
  EXPECT_TRUE(agent->IsAttachmentPending());

  // Produce a compositor frame. This should process the DOM mutations and
  // finish attaching the agent.
  Compositor().BeginFrame();

  EXPECT_FALSE(element->FastHasAttribute(html_names::kHiddenAttr));
  EXPECT_TRUE(agent->IsAttached());
}

// Ensure an annotation can target a content-visibility: auto section.
TEST_F(AnnotationAgentImplTest, ActivatesContentVisibilityAuto) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        position: absolute;
        contain: strict;
        width: 200px;
        height: 20px;
        left: 0;
        top: 2000px;
        content-visibility: auto;
      }
    </style>
    <p id="section">foobar</p>
  )HTML");

  Compositor().BeginFrame();

  auto* agent = CreateTextFinderAgent("foobar");

  Compositor().BeginFrame();

  EXPECT_TRUE(agent->IsAttachmentPending());

  // Produce a compositor frame. This should process the DOM mutations and
  // finish attaching the agent.
  Compositor().BeginFrame();

  EXPECT_TRUE(agent->IsAttached());

  Node* node =
      GetDocument().getElementById(AtomicString("section"))->firstChild();
  RangeInFlatTree* range = CreateRangeToExpectedText(node, 0, 6, "foobar");
  EXPECT_FALSE(DisplayLockUtilities::NeedsActivationForFindInPage(
      range->ToEphemeralRange()));
}

TEST_F(AnnotationAgentImplTest, NodeContentsBeginsWithLineBreak) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        position: absolute;
        width: 200px;
        height: 20px;
        left: 0;
        top: 20000px;
      }
    </style>
    <body>
      <div id="section">
        <div>Step</div>
        1
      </div>
    </body>
  )HTML");

  auto* element_foo =
      GetDocument().body()->getElementById(AtomicString("section"));
  ASSERT_NE(element_foo, nullptr);
  auto* agent = CreateNodeAgent(element_foo->GetDomNodeId());
  // Produce a compositor frame. This should process the DOM mutations and
  // finish attaching the agent.
  Compositor().BeginFrame();
  EXPECT_TRUE(agent->IsAttached());
  agent->ScrollIntoView(/*applies_focus=*/false);
  Compositor().BeginFrame();
  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

TEST_F(AnnotationAgentImplTest, NodeWithDisplayContents) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
     <style>
      #container {
        position: absolute;
        width: 200px;
        height: 20px;
        left: 0;
        top: 20000px;
      }
    </style>
    <body>
      <div id="container">
        <div id="contents" style="display: contents;">Text</div>
      </div>
    </body>
  )HTML");

  auto* div_with_display_contents =
      GetDocument().body()->getElementById(AtomicString("contents"));
  ASSERT_NE(div_with_display_contents, nullptr);
  auto* agent = CreateNodeAgent(div_with_display_contents->GetDomNodeId());
  // Produce a compositor frame. This should process the DOM mutations and
  // finish attaching the agent.
  Compositor().BeginFrame();
  EXPECT_TRUE(agent->IsAttached());
  agent->ScrollIntoView(/*applies_focus=*/false);
  EXPECT_TRUE(ExpectInViewport(*div_with_display_contents->firstChild()));
}

// kTextFinder type annotations must not cause side-effects. Ensure they do not
// expand a hidden=until-found element.
TEST_F(AnnotationAgentImplTest, TextFinderDoesntMutateDom) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p hidden="until-found" id="text">TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  Element* p = GetDocument().getElementById(AtomicString("text"));
  ASSERT_TRUE(p->FastHasAttribute(html_names::kHiddenAttr));

  auto* agent_foo =
      CreateTextFinderAgent("FOO", mojom::blink::AnnotationType::kTextFinder);
  ASSERT_TRUE(agent_foo);

  Compositor().BeginFrame();

  // Attachment should have succeeded but the <p> should remain hidden.
  ASSERT_TRUE(agent_foo->IsAttached());
  EXPECT_TRUE(p->FastHasAttribute(html_names::kHiddenAttr));

  // Sanity check that a shared highlight does un-hide the <p>
  auto* agent_bar = CreateTextFinderAgent(
      "BAR", mojom::blink::AnnotationType::kSharedHighlight);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_bar->IsAttachmentPending());
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_bar->IsAttached());
  EXPECT_FALSE(p->FastHasAttribute(html_names::kHiddenAttr));
}

// kTextFinder type annotations must not cause side-effects. Ensure they do not
// create document markers.
TEST_F(AnnotationAgentImplTest, TextFinderDoesntAddMarkers) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">TEST FOO PAGE BAR</p>
  )HTML");
  Compositor().BeginFrame();

  RangeInFlatTree* doc_range = CreateRangeForWholeDocument(GetDocument());
  ASSERT_EQ(NumMarkersInRange(*doc_range), 0ul);

  Node* p = GetDocument().getElementById(AtomicString("text"))->firstChild();
  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 5, 8, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kTextFinder);
  ASSERT_TRUE(agent_foo);

  Compositor().BeginFrame();

  // Attachment should have succeeded but no markers should be created.
  EXPECT_EQ(NumMarkersInRange(*doc_range), 0ul);

  // Sanity-check that a shared highlight does increase the marker count.
  RangeInFlatTree* range_bar = CreateRangeToExpectedText(p, 14, 17, "BAR");
  CreateAgentForRange(range_bar,
                      mojom::blink::AnnotationType::kSharedHighlight);
  Compositor().BeginFrame();
  EXPECT_EQ(NumMarkersInRange(*doc_range), 1ul);
}

// kTextFinder annotations should fail to find text within an empty
// overflow:hidden ancestor. This is a special case fix of
// https://crbug.com/1456392.
TEST_F(AnnotationAgentImplTest, TextFinderDoesntFindEmptyOverflowHidden) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id="container">
      <p id="text">FOO BAR</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Node* p = GetDocument().getElementById(AtomicString("text"))->firstChild();
  Element* container = GetDocument().getElementById(AtomicString("container"));
  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 0, 3, "FOO");

  // Empty container with `overflow: visible hidden` (y being hidden makes x
  // compute to auto).
  {
    container->setAttribute(
        html_names::kStyleAttr,
        AtomicString("height: 0px; overflow: visible hidden"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    // TextFinder should refuse to attach to the text since it has an empty,
    // overflow: hidden ancestor.
    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Empty container with `overflow: visible hidden` (y being hidden makes x
  // compute to auto). TextFinder should refuse to attach to the text since
  // it's clipped by the container.
  {
    container->setAttribute(
        html_names::kStyleAttr,
        AtomicString("height: 0px; overflow: visible hidden"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Empty container with `overflow: clip visible`. Should attach since
  // `overflow: clip` can clip in a single axis and in this case is clipping
  // the non-empty axis.
  {
    container->setAttribute(
        html_names::kStyleAttr,
        AtomicString("height: 0px; overflow: clip visible"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    EXPECT_TRUE(agent_foo->IsAttached());
  }

  // Empty container with clip on both axes. Shouldn't attach since it's
  // clipped in the empty direction.
  {
    container->setAttribute(html_names::kStyleAttr,
                            AtomicString("height: 0px; overflow: clip clip"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Empty container with `overflow: visible clip`. Should fail since
  // `overflow: clip` is in the empty direction
  {
    container->setAttribute(
        html_names::kStyleAttr,
        AtomicString("height: 0px; overflow: visible clip"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Giving the container size should make it visible to TextFinder annotations.
  {
    container->setAttribute(html_names::kStyleAttr,
                            AtomicString("height: 1px; overflow: hidden"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    // Now that the ancestor has size TextFinder should attach.
    EXPECT_TRUE(agent_foo->IsAttached());
  }

  // An empty container shouldn't prevent attaching if overflow is visible.
  {
    container->setAttribute(html_names::kStyleAttr,
                            AtomicString("height: 0px; overflow: visible"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    // Now that the ancestor has size TextFinder should attach.
    EXPECT_TRUE(agent_foo->IsAttached());
  }
}

// kTextFinder annotations should fail to find text within an opacity:0
// subtree.
TEST_F(AnnotationAgentImplTest, TextFinderDoesntFindOpacityZero) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        opacity: 0;
      }
    </style>
    <div id="container">
      <div>
        <p id="text">FOO BAR</p>
      </di>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Node* p = GetDocument().getElementById(AtomicString("text"))->firstChild();
  Element* container = GetDocument().getElementById(AtomicString("container"));
  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 0, 3, "FOO");

  {
    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    // TextFinder should refuse to attach to the text since it has an opacity:
    // 0 ancestor.
    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Ensure that setting the opacity to a non-zero value makes it findable.
  {
    container->setAttribute(html_names::kStyleAttr,
                            AtomicString("opacity: 0.1"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    EXPECT_TRUE(agent_foo->IsAttached());
  }
}

// kTextFinder annotations should fail to find text that's offscreen if it is
// in a position: fixed subtree.
TEST_F(AnnotationAgentImplTest, TextFinderDoesntFindOffscreenFixed) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        position:fixed;
        width: 100px;
        height: 20px;
        font: 10px/1 Ahem;
      }
      p {
        position: relative;
        margin: 0;
      }
    </style>
    <div id="container">
      <div>
        <p id="text">FOO BAR</p>
      </di>
    </div>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* p = GetDocument().getElementById(AtomicString("text"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(p->firstChild(), 0, 3, "FOO");

  // Ensure that putting the container offscreen makes the text unfindable.
  {
    container->setAttribute(html_names::kStyleAttr,
                            AtomicString("left: 0; top: -25px"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // The container partially intersects the viewport but the range doesn't.
  // This should still be considered unfindable.
  {
    container->setAttribute(html_names::kStyleAttr,
                            AtomicString("left: 0; top: -15px"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    // Text is still offscreen.
    ASSERT_LT(p->GetBoundingClientRect()->bottom(), 0);

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Push the <p> down so the text now intersects the viewport; this should
  // make it findable.
  {
    p->setAttribute(html_names::kStyleAttr, AtomicString("top: 10px"));

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    ASSERT_TRUE(agent_foo->NeedsAttachment());
    Compositor().BeginFrame();
    ASSERT_FALSE(agent_foo->NeedsAttachment());

    // Text is now within the viewport.
    ASSERT_GT(p->GetBoundingClientRect()->bottom(), 0);

    EXPECT_TRUE(agent_foo->IsAttached());
  }
}

TEST_F(AnnotationAgentImplTest, TextFinderFindsAcrossDisplayContents) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="1">
        <div id="2" style="display: contents;">
          <span>Hello </span>
          <span>world!</span>
        </div>
      </div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  auto* agent = CreateTextFinderAgent("Hello%20world!",
                                      mojom::blink::AnnotationType::kTextFinder);
  ASSERT_TRUE(agent->NeedsAttachment());

  Compositor().BeginFrame();
  EXPECT_TRUE(agent->IsAttached());
}

TEST_F(AnnotationAgentImplTest, GlicShouldAnimateScroll) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 1000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());
  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Invoking ScrollIntoView on the agent will trigger a scroll, but the scroll
  // will be animated (and so will not be in the viewport immediately).
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();
  Compositor().BeginFrame();
  EXPECT_TRUE(ExpectNotInViewport(*element_foo));

  // Start and complete the animation (which should happen within 700 ms based
  // on kDeltaBasedMaxDuration in scroll_offset_animation_curve.cc).
  task_environment().FastForwardBy(base::Milliseconds(16));
  Compositor().BeginFrame();
  task_environment().FastForwardBy(base::Milliseconds(700));
  Compositor().BeginFrame(/*time_delta_in_seconds*/ 0.7);

  // The text should now be in the viewport.
  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

TEST_F(AnnotationAgentImplTest, GlicShouldNotAnimateLongScroll) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 17000px;
      }
      body {
        height: 20000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());
  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Invoking ScrollIntoView on the agent will trigger an instant scroll
  // (without any animation), as the distance exceeds the threshold for a
  // smooth scroll.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();
  Compositor().BeginFrame();
  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

// Tests that the device scale factor is taken into account when deciding
// whether to smooth scroll. A device with a higher DSF will have a higher
// threshold for smooth scrolling.
TEST_F(AnnotationAgentImplTest, GlicScrollConsidersDeviceScaleFactor) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 5000px;
      }
      body {
        height: 10000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");
  WebView().MainFrameViewWidget()->SetDeviceScaleFactorForTesting(2.0);

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());
  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Invoking ScrollIntoView on the agent will trigger a scroll, but the scroll
  // will be animated (and so will not be in the viewport immediately).
  //
  // The scroll distance will be ~10,000 physical pixels (due to the device
  // scale factor), which would be above the threshold for a smooth scroll. But
  // if we normalize the distance using the device scale factor (which we
  // should), the scroll distance will be within the threshold.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();
  Compositor().BeginFrame();
  EXPECT_TRUE(ExpectNotInViewport(*element_foo));

  // Start and complete the animation (which should happen within 1500 ms based
  // on kProgrammaticScrollAnimationOverride's kMaxAnimationDuration in
  // cc/base/features.cc).
  task_environment().FastForwardBy(base::Milliseconds(16));
  Compositor().BeginFrame();
  task_environment().FastForwardBy(base::Milliseconds(700));
  Compositor().BeginFrame(/*time_delta_in_seconds=*/1.5);

  // The text should now be in the viewport.
  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

// Tests that browser zoom doesn't affect the computation of scroll behaviour
// for mojom::blink::AnnotationType::kGlic.
TEST_F(AnnotationAgentImplTest, GlicScrollIgnoresBrowserZoom) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        font: 10px/1 Ahem;
      }
      #foo {
        position: absolute;
        top: 10000px;
      }
      body {
        height: 20000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");
  // Sets the zoom factor to (1.2 ^ 4)x (i.e. ~2x).
  WebView().MainFrameViewWidget()->SetZoomLevelForTesting(4.0);

  LoadAhem();
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());
  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Invoking ScrollIntoView on the agent will trigger an instant scroll
  // (without any animation), as the distance exceeds the threshold for a
  // smooth scroll. The scroll distance will be >10,000 physical pixels (due to
  // browser zoom), which is above the threshold.
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();
  Compositor().BeginFrame();
  EXPECT_TRUE(ExpectInViewport(*element_foo));
}

// Ensure that a range that's valid in a flat-tree ordering but invalid in
// regular DOM order doesn't cause a crash. https://crbug.com/410033683.
TEST_F(AnnotationAgentImplTest, ValidFlatTreeRangeIsInvalidDOMRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <html>
      <head>
        <script>
          customElements.define(
            'my-component',
            class extends HTMLElement {
              constructor() {
                super();
                const template = document.getElementById('my-component-template');
                let templateContent = template.content;

                const shadowRoot = this.attachShadow({mode: 'open'});
                shadowRoot.appendChild(templateContent.cloneNode(true));
              }
            },
          );
        </script>
      </head>
      <body>
        <template id="my-component-template">
          <slot>
            <p>
              Temp
            </p>
          </slot>
          <p>
            After Slot
          </p>
        </template>

        <my-component>
          <!-- In a flat-tree traversal this node is treated as a child of the
               <slot> element (inside <my-component>'s shadow root) but in a
               regular tree traversal it is visited as part of the light-tree
               children of the <my-component> element. This swaps its order with
               respect to the "After Slot" <p>. -->
          <p>
            Match Text
          </p>
        </my-component>
      </body>
    </html>
  )HTML");

  LoadAhem();
  Compositor().BeginFrame();
  auto* agent = CreateTextFinderAgent("Match%20Text,After%20Slot");

  ASSERT_TRUE(agent->NeedsAttachment());
  Compositor().BeginFrame();
  ASSERT_FALSE(agent->NeedsAttachment());

  // Attachment will currently fail since DocumentMarkers doesn't support
  // FlatTree ranges.
  EXPECT_FALSE(agent->IsAttached());
}

// TODO(https://crbug.com/417493172): This should be parameterized for scroll
// types.
TEST_F(AnnotationAgentImplTest, GlicHighlight_SmokeTest) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST PAGE</p>
  )HTML");

  Compositor().BeginFrame();

  Node* text_node =
      GetDocument().getElementById(AtomicString("text"))->firstChild();

  RangeInFlatTree* range = CreateRangeToExpectedText(text_node, 0, 4, "TEST");
  auto* agent = CreateAgentForRange(range, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent);

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);
  ASSERT_FALSE(host.did_finish_attachment_rect_);

  // Finish the attachment.
  Compositor().BeginFrame();
  host.FlushForTesting();
  ASSERT_TRUE(host.did_finish_attachment_rect_);
  ASSERT_TRUE(agent->IsAttached());
  EXPECT_TRUE(GlicAnimationNotStarted());

  // The browser tells the renderer to scroll. Since we don't need to scroll,
  // the highlight animation starts immediately, which queues the first
  // RequestAnimationFrame.
  agent->ScrollIntoView(/*applies_focus=*/false);
  EXPECT_TRUE(GlicAnimationRunning());

  // Execute the first RequestAnimationFrame, which initializes the T=0.
  Compositor().BeginFrame();

  // T=500ms. `FastForwardBy()` makes sure we skip the `sleep()` inside
  // `BeginFrame()`.
  task_environment().FastForwardBy(base::Milliseconds(500));
  Compositor().BeginFrame(0.5);
  EXPECT_TRUE(GlicAnimationRunning());

  // T=1100ms. Submit the last frame with progress=1.
  task_environment().FastForwardBy(base::Milliseconds(600));
  Compositor().BeginFrame(0.6);
  EXPECT_TRUE(GlicAnimationFinished());

  const auto& markers = GetAllMarkers();
  EXPECT_EQ(markers.size(), 1u);
  EXPECT_EQ(markers[0]->GetType(), DocumentMarker::MarkerType::kGlic);
  EXPECT_EQ(
      To<GlicMarker>(markers[0].Get())->BackgroundColor(),
      Color::FromRGBA32(shared_highlighting::kFragmentTextBackgroundColorARGB));
}

// Test that the highlight animation is stopped when the agent is removed from
// the container. This can happen when the client dispose the agent.
TEST_F(AnnotationAgentImplTest, GlicHighlight_StopOnAgentRemoval) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST PAGE</p>
  )HTML");

  Compositor().BeginFrame();

  Node* text_node =
      GetDocument().getElementById(AtomicString("text"))->firstChild();

  RangeInFlatTree* range = CreateRangeToExpectedText(text_node, 0, 4, "TEST");
  auto* agent = CreateAgentForRange(range, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent);

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);
  ASSERT_FALSE(host.did_finish_attachment_rect_);
  Compositor().BeginFrame();
  host.FlushForTesting();
  ASSERT_TRUE(host.did_finish_attachment_rect_);
  ASSERT_TRUE(agent->IsAttached());

  // The browser tells the renderer to scroll. Since we don't need to scroll,
  // the highlight animation starts immediately, which queues the first
  // RequestAnimationFrame.
  agent->ScrollIntoView(/*applies_focus=*/false);

  EXPECT_EQ(GetAllMarkers().size(), 1u);

  agent->OwningContainer()->RemoveAgent(*agent);

  Compositor().BeginFrame();

  EXPECT_TRUE(GetAllMarkers().empty());
}

// Regression test to make sure nothing crashes if the highlighted text node is
// removed during the animation.
TEST_F(AnnotationAgentImplTest, GlicHighlight_HighlightAfterTextRemoval) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>THE <span id='span'>TEST </span>PAGE</p>
  )HTML");

  Compositor().BeginFrame();

  RangeInFlatTree* text_range = CreateRangeForWholeDocument(GetDocument());
  auto* test_agent =
      CreateAgentForRange(text_range, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(test_agent);

  MockAnnotationAgentHost test_host;
  test_host.BindToAgent(*test_agent);
  ASSERT_FALSE(test_host.did_finish_attachment_rect_);

  Compositor().BeginFrame();
  test_host.FlushForTesting();
  ASSERT_TRUE(test_host.did_finish_attachment_rect_);

  GetDocument().getElementById(AtomicString{"span"})->remove();
  Compositor().BeginFrame();
  Element* text_element = GetDocument().getElementById(AtomicString{"text"});
  EXPECT_EQ(GetDocument()
                .Markers()
                .MarkersFor(*To<Text>(text_element->firstChild()))
                .size(),
            1u);
  EXPECT_EQ(
      GetDocument()
          .Markers()
          .MarkersFor(*To<Text>(text_element->firstChild()->nextSibling()))
          .size(),
      1u);

  text_element->remove();
  Compositor().BeginFrame();
  // The markers are kept alive despite all the text nodes are removed.
  EXPECT_EQ(GetAllMarkers().size(), 3u);
}

// Test that the removal of a different agent does not affect the glic highlight
// agent.
TEST_F(AnnotationAgentImplTest, GlicHighlight_MultipleAgentRemoval) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text'>TEST PAGE</p>
  )HTML");

  Compositor().BeginFrame();

  Element* element_text = GetDocument().getElementById(AtomicString("text"));

  RangeInFlatTree* range =
      CreateRangeToExpectedText(element_text->firstChild(), 0, 4, "TEST");
  auto* glic_agent =
      CreateAgentForRange(range, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(glic_agent);
  auto* user_note_agent = CreateAgentForRange(
      range, mojom::blink::AnnotationType::kSharedHighlight);
  ASSERT_TRUE(user_note_agent);

  MockAnnotationAgentHost glic_host;
  glic_host.BindToAgent(*glic_agent);

  MockAnnotationAgentHost user_note_host;
  user_note_host.BindToAgent(*user_note_agent);

  Compositor().BeginFrame();
  glic_host.FlushForTesting();
  user_note_host.FlushForTesting();
  ASSERT_TRUE(user_note_agent->IsAttached());
  ASSERT_TRUE(glic_agent->IsAttached());

  EXPECT_EQ(GetAllMarkers().size(), 2u);

  AnnotationAgentContainerImpl* container =
      AnnotationAgentContainerImpl::FromIfExists(GetDocument());
  ASSERT_TRUE(container);

  container->RemoveAgent(*user_note_agent);
  const auto& markers = GetAllMarkers();
  ASSERT_EQ(markers.size(), 1u);
  EXPECT_EQ(markers[0]->GetType(), DocumentMarker::MarkerType::kGlic);

  container->RemoveAgent(*glic_agent);
  EXPECT_TRUE(GetAllMarkers().empty());
}

// Test that we always start the highlight animation on a clean state.
TEST_F(AnnotationAgentImplTest, GlicHighlight_ResetStateOnNewTextNodes) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='text1'>TEST </p>
    <p id='text2'>PAGE</p>
  )HTML");

  Compositor().BeginFrame();

  Element* text1 = GetDocument().getElementById(AtomicString("text1"));
  RangeInFlatTree* range1 =
      CreateRangeToExpectedText(text1->firstChild(), 0, 4, "TEST");
  auto* agent1 =
      CreateAgentForRange(range1, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent1);
  MockAnnotationAgentHost host1;
  host1.BindToAgent(*agent1);
  Compositor().BeginFrame();
  host1.FlushForTesting();
  ASSERT_TRUE(agent1->IsAttached());

  // The browser tells the renderer to scroll. Since we don't need to scroll,
  // the highlight animation starts immediately, which queues the first
  // RequestAnimationFrame.
  agent1->ScrollIntoView(/*applies_focus=*/false);
  // Execute the first RequestAnimationFrame with T=0.
  Compositor().BeginFrame();
  // Execute the first RequestAnimationFrame with T=0.5.
  task_environment().FastForwardBy(base::Milliseconds(500));
  Compositor().BeginFrame(0.5);

  // Greater than 0.
  EXPECT_GT(GetAlphaForGlicMarkerAt(0u), 0.f);

  // Simulate that glic highlights a different text. Currently only one text
  // (agent) is highlighted at a time.
  agent1->OwningContainer()->RemoveAgent(*agent1);
  EXPECT_TRUE(GetAllMarkers().empty());

  // Add a second agent while the highlight from the first one is still
  // animating (removing the agent doesn't stop the animation) its marker
  // highlight. Ensure the new animation uses a fresh state.
  Element* text2 = GetDocument().getElementById(AtomicString("text2"));
  RangeInFlatTree* range2 =
      CreateRangeToExpectedText(text2->firstChild(), 0, 4, "PAGE");
  auto* agent2 =
      CreateAgentForRange(range2, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent2);
  MockAnnotationAgentHost host2;
  host2.BindToAgent(*agent2);
  Compositor().BeginFrame();
  host2.FlushForTesting();
  ASSERT_TRUE(agent2->IsAttached());

  // No scroll. Queues the first RequestAnimationFrame.
  agent2->ScrollIntoView(/*applies_focus=*/false);
  // Execute the first RequestAnimationFrame with T=0.
  task_environment().FastForwardBy(base::Milliseconds(16));
  Compositor().BeginFrame();

  EXPECT_EQ(GetAlphaForGlicMarkerAt(0u), 0.f);
}

TEST_F(AnnotationAgentImplTest,
       GlicHighlight_HighLightStartsAfterScrollFinishes) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #foo {
        position: absolute;
        top: 1000px;
      }
      body {
        height: 5000px;
        margin: 0;
      }
    </style>
    <p id='foo'>FOO<p>
  )HTML");
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  EXPECT_TRUE(GlicAnimationNotStarted());

  // We need to scroll.
  EXPECT_TRUE(ExpectNotInViewport(*element_foo));
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  Compositor().BeginFrame();
  Compositor().BeginFrame();

  EXPECT_TRUE(GlicAnimationNotStarted());

  // Smooth scrolling is guaranteed to finish within 1500ms.
  task_environment().FastForwardBy(base::Seconds(2));
  Compositor().BeginFrame(1.0);
  EXPECT_TRUE(ExpectInViewport(*element_foo));
  // Since the text node is in the viewport, we must have queued the first
  // RequestAnimationFrame.
  EXPECT_TRUE(GlicAnimationRunning());
  EXPECT_EQ(GetAlphaForGlicMarkerAt(0u), 0.f);

  // Run the highlight animation.
  task_environment().FastForwardBy(base::Milliseconds(16));
  Compositor().BeginFrame();

  EXPECT_TRUE(GlicAnimationRunning());
  // The opacity is greater than 0.
  EXPECT_GT(GetAlphaForGlicMarkerAt(0u), 0.f);
}

// Glic instant scrolls for more than 15000dip of distance.
TEST_F(AnnotationAgentImplTest, GlicHighlight_InstantStartForInstantScroll) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #foo {
      position: absolute;
      top: 17000px;
    }
    body {
      height: 20000px;
      margin: 0;
    }
    </style>
    <p id='foo'>FOO<p>
  )HTML");
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  EXPECT_TRUE(GlicAnimationNotStarted());

  // We need to scroll.
  EXPECT_TRUE(ExpectNotInViewport(*element_foo));
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  // Instant scroll.
  EXPECT_TRUE(ExpectInViewport(*element_foo));
  EXPECT_TRUE(GlicAnimationRunning());

  // Queue the first RequestAnimationFrame.
  task_environment().FastForwardBy(base::Milliseconds(16));
  Compositor().BeginFrame();
  // Run the first RequestAnimationFrame.
  task_environment().FastForwardBy(base::Milliseconds(16));
  Compositor().BeginFrame();

  EXPECT_TRUE(GlicAnimationRunning());
  // The opacity is greater than 0.
  EXPECT_GT(GetAlphaForGlicMarkerAt(0u), 0.f);
}

// Test that the highlight doesn't restart after subsequent scrollEnd events.
TEST_F(AnnotationAgentImplTest,
       GlicHighlight_AnimationDoesNotRestartAfterSubsequentScroll) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #foo {
      position: absolute;
      top: 2000px;
    }
    #bar {
      position: absolute;
      top: 3000px;
    }
    body {
      height: 4000px;
      margin: 0;
    }
    </style>
    <p id='foo'>FOO<p>
    <p id='bar'>BAR<p>
  )HTML");
  Compositor().BeginFrame();

  Element* element_foo = GetDocument().getElementById(AtomicString("foo"));

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo->firstChild(), 0, 3, "FOO");
  auto* agent_foo =
      CreateAgentForRange(range_foo, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent_foo);

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  Compositor().BeginFrame();
  ASSERT_TRUE(agent_foo->IsAttached());

  EXPECT_TRUE(GlicAnimationNotStarted());

  // We need to scroll.
  EXPECT_TRUE(ExpectNotInViewport(*element_foo));
  host_foo.agent_->ScrollIntoView(/*applies_focus=*/false);
  host_foo.FlushForTesting();

  Compositor().BeginFrame();
  Compositor().BeginFrame();
  EXPECT_TRUE(GlicAnimationNotStarted());

  // Max smooth scrolling is capped at 1500ms.
  task_environment().FastForwardBy(base::Seconds(2));
  Compositor().BeginFrame(1.0);
  EXPECT_TRUE(ExpectInViewport(*element_foo));
  // Since the text node is in the viewport, we must have queued the first
  // RequestAnimationFrame.
  EXPECT_TRUE(GlicAnimationRunning());

  // Run the highlight animation, and finish it.
  task_environment().FastForwardBy(base::Seconds(1));
  Compositor().BeginFrame(/*time_delta_in_seconds=*/1);
  EXPECT_TRUE(GlicAnimationFinished());
  EXPECT_EQ(GetAlphaForGlicMarkerAt(0u), 1.f);

  // Scroll BAR into the viewport.
  Element* element_bar = GetDocument().getElementById(AtomicString("bar"));
  EXPECT_TRUE(ExpectNotInViewport(*element_bar));
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock(V8ScrollLogicalPosition::Enum::kStart);
  options->setBehavior(V8ScrollBehavior::Enum::kInstant);
  auto* arg =
      MakeGarbageCollected<V8UnionBooleanOrScrollIntoViewOptions>(options);
  element_bar->scrollIntoViewForTesting(arg);
  Compositor().BeginFrame();
  EXPECT_TRUE(ExpectInViewport(*element_bar));

  // If we started another highlight animation for scrolling BAR, the animation
  // should not have restarted.
  EXPECT_TRUE(GlicAnimationFinished());
  EXPECT_EQ(GetAlphaForGlicMarkerAt(0u), 1.f);
}

namespace {
enum class ScrollType {
  kNoScroll = 0,
  kSmoothScroll,
  kInstantScroll,
};

struct GlicScrollBehaviorConfig {
  ScrollType type;
  int element_top;
  int body_height;
};

void PrintTo(const GlicScrollBehaviorConfig& v, std::ostream* os) {
  std::string type;
  switch (v.type) {
    case ScrollType::kNoScroll:
      type = "NoScroll";
      break;
    case ScrollType::kSmoothScroll:
      type = "SmoothScroll";
      break;
    case ScrollType::kInstantScroll:
      type = "InstantScroll";
      break;
  }
  *os << type;
}
}  // namespace

class AnnotationAgentImplTestWithScrollingBehavior
    : public AnnotationAgentImplTest,
      public ::testing::WithParamInterface<GlicScrollBehaviorConfig> {
 public:
  AnnotationAgentImplTestWithScrollingBehavior() = default;
  ~AnnotationAgentImplTestWithScrollingBehavior() override = default;
};

// Assert that there are highlight animations when prefers-reduced-motion is
// enabled, and SmoothScroll becomes instant.
TEST_P(AnnotationAgentImplTestWithScrollingBehavior,
       NoAnimationWithPrefersReducedMotion) {
  GlicScrollBehaviorConfig config = GetParam();
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(String::Format(R"HTML(
      <!DOCTYPE html>
      <style>
        #foo {
          position: absolute;
          top: %dpx;
        }
        body {
          height: %dpx;
          margin: 0;
        }
      </style>
      <p id='foo'>FOO<p>
    )HTML", config.element_top, config.body_height)
  );

  Compositor().BeginFrame();

  // Enables prefers-reduced-motion.
  WebViewImpl* web_view =
      GetDocument().GetPage()->GetChromeClient().GetWebView();
  web_view->GetSettings()->SetPrefersReducedMotion(true);

  Element* foo_element = GetDocument().getElementById(AtomicString("foo"));
  RangeInFlatTree* range =
      CreateRangeToExpectedText(foo_element->firstChild(), 0, 3, "FOO");
  auto* agent = CreateAgentForRange(range, mojom::blink::AnnotationType::kGlic);
  ASSERT_TRUE(agent);

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);
  ASSERT_FALSE(host.did_finish_attachment_rect_);

  // Finish the attachment.
  Compositor().BeginFrame();
  host.FlushForTesting();
  ASSERT_TRUE(host.did_finish_attachment_rect_);
  ASSERT_TRUE(agent->IsAttached());
  ASSERT_TRUE(GlicAnimationNotStarted());

  switch (GetParam().type) {
    case ScrollType::kNoScroll: {
      // The browser tells the renderer to scroll. Since we don't need to
      // scroll, the highlight animation starts immediately, which sets the
      // opacity to full.
      agent->ScrollIntoView(/*applies_focus=*/false);
      EXPECT_TRUE(ExpectInViewport(*foo_element));
      break;
    }
    case ScrollType::kSmoothScroll:
      // We would ordinarily smooth scroll here. Since prefers-reduced-motion is
      // enabled, the scroll is instant.
    case ScrollType::kInstantScroll: {
      // We need to scroll.
      EXPECT_TRUE(ExpectNotInViewport(*foo_element));
      host.agent_->ScrollIntoView(/*applies_focus=*/false);
      host.FlushForTesting();
      // Instant scroll.
      EXPECT_TRUE(ExpectInViewport(*foo_element));
      break;
    }
  }

  EXPECT_TRUE(GlicAnimationFinished());
  EXPECT_EQ(GetAlphaForGlicMarkerAt(0u), 1.f);
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    AnnotationAgentImplTestWithScrollingBehavior,
    ::testing::Values(
        GlicScrollBehaviorConfig{.type = ScrollType::kNoScroll,
                                 .element_top = 100,
                                 .body_height = 200},
        GlicScrollBehaviorConfig{.type = ScrollType::kSmoothScroll,
                                 .element_top = 1000,
                                 .body_height = 5000},
        GlicScrollBehaviorConfig{.type = ScrollType::kInstantScroll,
                                 .element_top = 17000,
                                 .body_height = 20000}),
    ::testing::PrintToStringParamName());

}  // namespace blink
