// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"

#include <gtest/gtest.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_test_utils.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
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
  // Helper to create a range to some text within a single element. Verifies
  // the Range selects the `expected` text.
  RangeInFlatTree* CreateRangeToExpectedText(Element* element,
                                             int start_offset,
                                             int end_offset,
                                             const String& expected) {
    EXPECT_TRUE(element);
    if (!element)
      return nullptr;

    const auto& range_start = Position(element->firstChild(), start_offset);
    const auto& range_end = Position(element->firstChild(), end_offset);

    String actual = PlainText(EphemeralRange(range_start, range_end));
    EXPECT_EQ(expected, actual);
    if (expected != actual)
      return nullptr;

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

    auto* container = AnnotationAgentContainerImpl::From(GetDocument());
    EXPECT_TRUE(container);
    if (!container)
      return nullptr;

    auto* mock_selector = MakeGarbageCollected<MockAnnotationSelector>(*range);
    return container->CreateUnboundAgent(type, *mock_selector);
  }

  // Creates an agent with a mock selector that will always fail to find a
  // range when attaching.
  AnnotationAgentImpl* CreateAgentFailsAttach() {
    auto* container = AnnotationAgentContainerImpl::From(GetDocument());
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
    scoped_refptr<SharedBuffer> shared_buffer =
        test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
    auto* buffer =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            DOMArrayBuffer::Create(shared_buffer));
    FontFace* ahem =
        FontFace::Create(GetDocument().GetFrame()->DomWindow(), "Ahem", buffer,
                         FontFaceDescriptors::Create());

    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    DummyExceptionStateForTesting exception_state;
    FontFaceSetDocument::From(GetDocument())
        ->addForBinding(script_state, ahem, exception_state);
  }
};

// Tests that the agent type is correctly set.
TEST_F(AnnotationAgentImplTest, AgentType) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
  ASSERT_TRUE(container);
  auto* shared_highlight_agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  auto* user_note_agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kUserNote,
      *MakeGarbageCollected<MockAnnotationSelector>());

  EXPECT_EQ(GetAgentType(shared_highlight_agent),
            mojom::blink::AnnotationType::kSharedHighlight);
  EXPECT_EQ(GetAgentType(user_note_agent),
            mojom::blink::AnnotationType::kUserNote);
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

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
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

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
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

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);

  ASSERT_TRUE(host.agent_.is_connected());
  ASSERT_FALSE(host.did_disconnect_);

  agent->Remove();
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

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  EXPECT_FALSE(IsRemoved(agent));

  agent->Attach();
  ASSERT_TRUE(agent->IsAttached());

  agent->Remove();

  EXPECT_TRUE(IsRemoved(agent));
  EXPECT_FALSE(agent->IsAttached());
}

// Tests that attaching an agent to DOM in the document happens synchronously.
TEST_F(AnnotationAgentImplTest, AttachIsSynchronous) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
  ASSERT_TRUE(container);
  auto* agent = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  agent->Attach();
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

  Element* p = GetDocument().getElementById("text");

  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 5, 8, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar = CreateRangeToExpectedText(p, 14, 17, "BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  agent_foo->Attach();
  ASSERT_TRUE(agent_foo->IsAttached());

  // A marker should have been created on "FOO" but not yet on "BAR".
  EXPECT_EQ(NumMarkersInRange(*range_foo), 1ul);
  EXPECT_EQ(NumMarkersInRange(*range_bar), 0ul);

  agent_bar->Attach();
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

  Element* p = GetDocument().getElementById("text");

  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 5, 8, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar = CreateRangeToExpectedText(p, 14, 17, "BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  agent_foo->Attach();
  agent_bar->Attach();
  ASSERT_EQ(NumMarkersInRange(*range_foo), 1ul);
  ASSERT_EQ(NumMarkersInRange(*range_bar), 1ul);

  agent_foo->Remove();

  ASSERT_EQ(NumMarkersInRange(*range_foo), 0ul);
  ASSERT_EQ(NumMarkersInRange(*range_bar), 1ul);

  agent_bar->Remove();

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

  auto* agent = CreateAgentFailsAttach();
  ASSERT_TRUE(agent);

  Element* p = GetDocument().getElementById("text");
  RangeInFlatTree* range =
      CreateRangeToExpectedText(p, 0, 17, "TEST FOO PAGE BAR");
  ASSERT_EQ(NumMarkersInRange(*range), 0ul);

  agent->Attach();

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

  auto* agent = CreateAgentFailsAttach();
  ASSERT_TRUE(agent);

  MockAnnotationAgentHost host;
  host.BindToAgent(*agent);

  ASSERT_FALSE(host.did_disconnect_);
  ASSERT_TRUE(host.agent_.is_connected());
  ASSERT_FALSE(host.did_finish_attachment_rect_);

  agent->Attach();
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

  Element* element_text = GetDocument().getElementById("text");

  auto* agent = CreateAgentFailsAttach();
  ASSERT_TRUE(agent);
  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_text, 5, 13, "FOO PAGE");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar =
      CreateRangeToExpectedText(element_text, 9, 17, "PAGE BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  MockAnnotationAgentHost host_foo;
  MockAnnotationAgentHost host_bar;
  host_foo.BindToAgent(*agent_foo);
  host_bar.BindToAgent(*agent_bar);

  ASSERT_FALSE(host_foo.did_finish_attachment_rect_);
  ASSERT_FALSE(host_bar.did_finish_attachment_rect_);

  agent_foo->Attach();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  EXPECT_TRUE(host_foo.did_finish_attachment_rect_);
  ASSERT_FALSE(host_bar.did_finish_attachment_rect_);

  agent_bar->Attach();
  ASSERT_TRUE(agent_bar->IsAttached());

  host_bar.FlushForTesting();
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
        mojom::blink::ScrollBehavior::kInstant,
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

  Element* element_foo = GetDocument().getElementById("foo");
  Element* element_bar = GetDocument().getElementById("bar");

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo, 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  RangeInFlatTree* range_bar =
      CreateRangeToExpectedText(element_bar, 0, 3, "BAR");
  auto* agent_bar = CreateAgentForRange(range_bar);
  ASSERT_TRUE(agent_bar);

  MockAnnotationAgentHost host_foo;
  MockAnnotationAgentHost host_bar;

  host_foo.BindToAgent(*agent_foo);
  host_bar.BindToAgent(*agent_bar);

  ASSERT_FALSE(host_foo.did_finish_attachment_rect_);
  ASSERT_FALSE(host_bar.did_finish_attachment_rect_);

  agent_foo->Attach();
  EXPECT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  ASSERT_TRUE(host_foo.did_finish_attachment_rect_);
  EXPECT_EQ(*host_foo.did_finish_attachment_rect_, gfx::Rect(0, 1010, 30, 10));
  ASSERT_FALSE(host_bar.did_finish_attachment_rect_);

  agent_bar->Attach();
  EXPECT_TRUE(agent_bar->IsAttached());

  host_bar.FlushForTesting();

  ASSERT_TRUE(host_bar.did_finish_attachment_rect_);
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

  Element* element_foo = GetDocument().getElementById("foo");

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo, 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  agent_foo->Attach();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Attachment must not cause any scrolling.
  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView();
  host_foo.FlushForTesting();

  EXPECT_TRUE(ExpectInViewport(*element_foo));
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

  Element* element_foo = GetDocument().getElementById("foo");

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_foo, 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectNotInViewport(*element_foo));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  agent_foo->Attach();
  ASSERT_TRUE(agent_foo->IsAttached());

  host_foo.FlushForTesting();

  // Invoking ScrollIntoView on the agent should cause the attached content
  // into the viewport.
  host_foo.agent_->ScrollIntoView();
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

  Element* element_text = GetDocument().getElementById("text");

  RangeInFlatTree* range_foo =
      CreateRangeToExpectedText(element_text, 0, 3, "FOO");
  auto* agent_foo = CreateAgentForRange(range_foo);
  ASSERT_TRUE(agent_foo);

  ASSERT_TRUE(ExpectInViewport(*element_text));
  ASSERT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset(),
            ScrollOffset());

  MockAnnotationAgentHost host_foo;
  host_foo.BindToAgent(*agent_foo);
  agent_foo->Attach();
  ASSERT_TRUE(agent_foo->IsAttached());
  host_foo.FlushForTesting();

  element_text->setAttribute(html_names::kStyleAttr, "top: 2000px");

  // Invoking ScrollIntoView on the agent should perform layout and then cause
  // the attached content to scroll into the viewport.
  host_foo.agent_->ScrollIntoView();
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

  Element* element_text = GetDocument().getElementById("text");

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
  agent->Attach();

  // Attachment should fail for this collapsed range.
  EXPECT_FALSE(agent->IsAttached());
  host.FlushForTesting();

  // Ensure calling ScrollIntoView doesn't crash.
  host.agent_->ScrollIntoView();
  host.FlushForTesting();
  EXPECT_EQ(GetDocument().View()->GetRootFrameViewport()->GetScrollOffset().y(),
            0);
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

  Element* p = GetDocument().getElementById("text");
  Element* container = GetDocument().getElementById("container");
  RangeInFlatTree* range_foo = CreateRangeToExpectedText(p, 0, 3, "FOO");

  // Empty container with `overflow: visible hidden` (y being hidden makes x
  // compute to auto).
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 0px; overflow: visible hidden");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    // TextFinder should refuse to attach to the text since it has an empty,
    // overflow: hidden ancestor.
    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Empty container with `overflow: visible hidden` (y being hidden makes x
  // compute to auto). TextFinder should refuse to attach to the text since
  // it's clipped by the container.
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 0px; overflow: visible hidden");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Empty container with `overflow: clip visible`. Should attach since
  // `overflow: clip` can clip in a single axis and in this case is clipping
  // the non-empty axis.
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 0px; overflow: clip visible");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    EXPECT_TRUE(agent_foo->IsAttached());
  }

  // Empty container with clip on both axes. Shouldn't attach since it's
  // clipped in the empty direction.
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 0px; overflow: clip clip");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Empty container with `overflow: visible clip`. Should fail since
  // `overflow: clip` is in the empty direction
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 0px; overflow: visible clip");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    EXPECT_FALSE(agent_foo->IsAttached());
  }

  // Giving the container size should make it visible to TextFinder annotations.
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 1px; overflow: hidden");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    // Now that the ancestor has size TextFinder should attach.
    EXPECT_TRUE(agent_foo->IsAttached());
  }

  // An empty container shouldn't prevent attaching if overflow is visible.
  {
    container->setAttribute(html_names::kStyleAttr,
                            "height: 0px; overflow: visible");
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    auto* agent_foo = CreateAgentForRange(
        range_foo, mojom::blink::AnnotationType::kTextFinder);
    agent_foo->Attach();

    // Now that the ancestor has size TextFinder should attach.
    EXPECT_TRUE(agent_foo->IsAttached());
  }
}

}  // namespace blink
