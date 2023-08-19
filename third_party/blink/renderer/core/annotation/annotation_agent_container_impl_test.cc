// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"

#include <gtest/gtest.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_test_utils.h"
#include "third_party/blink/renderer/core/editing/finder/async_find_buffer.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class AnnotationAgentContainerImplTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

 protected:
  bool IsInContainer(AnnotationAgentImpl& agent,
                     AnnotationAgentContainerImpl& container) const {
    return container.agents_.Contains(&agent);
  }

  size_t GetAgentCount(AnnotationAgentContainerImpl& container) {
    return container.agents_.size();
  }

  void SendRightClick(const gfx::Point& click_point) {
    auto event = frame_test_helpers::CreateMouseEvent(
        WebMouseEvent::Type::kMouseDown, WebMouseEvent::Button::kRight,
        click_point, /*modifiers=*/0);
    event.click_count = 1;
    WebView().MainFrameViewWidget()->HandleInputEvent(
        WebCoalescedInputEvent(event, ui::LatencyInfo()));
  }

  ScopedUseMockAnnotationSelector use_mock_annotation_selector_;
};

// Ensure containers aren't supplementing a document until they're requested.
TEST_F(AnnotationAgentContainerImplTest, IsConstructedLazily) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/subframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
    <iframe src="https://example.com/subframe.html"></iframe>
  )HTML");
  child_request.Complete(R"HTML(
    <!DOCTYPE html>
    SUBFRAME
  )HTML");
  Compositor().BeginFrame();

  ASSERT_TRUE(GetDocument().GetFrame());
  ASSERT_TRUE(GetDocument().GetFrame()->FirstChild());
  LocalFrame* child_frame =
      DynamicTo<LocalFrame>(GetDocument().GetFrame()->FirstChild());
  ASSERT_TRUE(child_frame);

  Document* child_document = child_frame->GetDocument();
  ASSERT_TRUE(child_document);

  // Initially, the container supplement should not exist on either document.
  EXPECT_FALSE(AnnotationAgentContainerImpl::FromIfExists(GetDocument()));
  EXPECT_FALSE(AnnotationAgentContainerImpl::FromIfExists(*child_document));

  // Calling the getter on the container should create the supplement but only
  // for the child document.
  auto* child_container =
      AnnotationAgentContainerImpl::CreateIfNeeded(*child_document);
  EXPECT_TRUE(child_container);
  EXPECT_EQ(child_container,
            AnnotationAgentContainerImpl::FromIfExists(*child_document));
  EXPECT_FALSE(AnnotationAgentContainerImpl::FromIfExists(GetDocument()));

  // Calling the getter for the main document should now create that supplement.
  auto* main_container =
      AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  EXPECT_EQ(main_container,
            AnnotationAgentContainerImpl::FromIfExists(GetDocument()));

  // The child and main documents should each have their own containers.
  EXPECT_NE(main_container, child_container);
}

// Test that binding the mojo interface creates a new container.
TEST_F(AnnotationAgentContainerImplTest, BindingCreatesContainer) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  mojo::Remote<mojom::blink::AnnotationAgentContainer> remote;
  ASSERT_FALSE(remote.is_bound());
  ASSERT_FALSE(AnnotationAgentContainerImpl::FromIfExists(GetDocument()));

  AnnotationAgentContainerImpl::BindReceiver(
      GetDocument().GetFrame(), remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(remote.is_connected());
  EXPECT_TRUE(AnnotationAgentContainerImpl::FromIfExists(GetDocument()));
}

// Test that navigating to a new document breaks the binding on the old
// document's container.
TEST_F(AnnotationAgentContainerImplTest, NavigationBreaksBinding) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest request_next("https://example.com/next.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  mojo::Remote<mojom::blink::AnnotationAgentContainer> remote;
  AnnotationAgentContainerImpl::BindReceiver(
      GetDocument().GetFrame(), remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote.is_connected());

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");
  Compositor().BeginFrame();

  remote.FlushForTesting();

  EXPECT_FALSE(remote.is_connected());
}

// Test that navigating to a new document installs a new container.
TEST_F(AnnotationAgentContainerImplTest, NavigationReplacesContainer) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest request_next("https://example.com/next.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");
  Compositor().BeginFrame();

  auto* container_next =
      AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  EXPECT_NE(container, container_next);
}

// Test that the container can create an unbound agent.
TEST_F(AnnotationAgentContainerImplTest, CreateUnboundAgent) {
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

  EXPECT_TRUE(agent);
  EXPECT_FALSE(agent->IsBoundForTesting());
  EXPECT_FALSE(agent->IsAttached());

  EXPECT_TRUE(IsInContainer(*agent, *container));
}

// Test that the container can create a bound agent. It should automatically
// perform attachment at creation time.
TEST_F(AnnotationAgentContainerImplTest, CreateBoundAgent) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  MockAnnotationAgentHost host;
  auto remote_receiver_pair = host.BindForCreateAgent();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  container->CreateAgent(std::move(remote_receiver_pair.first),
                         std::move(remote_receiver_pair.second),
                         mojom::blink::AnnotationType::kSharedHighlight,
                         "MockAnnotationSelector");

  EXPECT_EQ(GetAgentCount(*container), 1ul);

  EXPECT_TRUE(host.agent_.is_connected());

  // Creating an agent from selection should automatically attach, which will
  // happen in the next BeginFrame.
  Compositor().BeginFrame();
  host.FlushForTesting();
  EXPECT_TRUE(host.did_finish_attachment_rect_);
  EXPECT_FALSE(host.did_disconnect_);
}

// Test that creating an agent in a document that hasn't yet completed parsing
// will cause agents to defer attachment and attempt it when the document
// finishes parsing.
TEST_F(AnnotationAgentContainerImplTest, DeferAttachmentUntilFinishedParsing) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Write(R"HTML(
    <!DOCTYPE html>
    <body>TEST PAGE</body>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_FALSE(GetDocument().HasFinishedParsing());

  MockAnnotationAgentHost host;
  auto remote_receiver_pair = host.BindForCreateAgent();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  container->CreateAgent(std::move(remote_receiver_pair.first),
                         std::move(remote_receiver_pair.second),
                         mojom::blink::AnnotationType::kUserNote,
                         "MockAnnotationSelector");

  // The agent should be created and bound.
  EXPECT_EQ(GetAgentCount(*container), 1ul);
  EXPECT_TRUE(host.agent_.is_connected());

  // Attachment should not have been attempted yet.
  host.FlushForTesting();
  EXPECT_FALSE(host.did_finish_attachment_rect_);
  EXPECT_FALSE(host.did_disconnect_);

  request.Finish();
  ASSERT_TRUE(GetDocument().HasFinishedParsing());

  // Now that parsing finished, attachment should be completed.
  host.FlushForTesting();
  EXPECT_TRUE(host.did_finish_attachment_rect_);
}

// Test that an agent removing itself also removes it from its container.
TEST_F(AnnotationAgentContainerImplTest, ManuallyRemoveAgent) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_TRUE(container);
  auto* agent1 = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());
  auto* agent2 = container->CreateUnboundAgent(
      mojom::blink::AnnotationType::kSharedHighlight,
      *MakeGarbageCollected<MockAnnotationSelector>());

  ASSERT_TRUE(agent1);
  ASSERT_TRUE(agent2);
  EXPECT_EQ(GetAgentCount(*container), 2ul);
  EXPECT_TRUE(IsInContainer(*agent1, *container));
  EXPECT_TRUE(IsInContainer(*agent2, *container));

  agent1->Remove();

  EXPECT_EQ(GetAgentCount(*container), 1ul);
  EXPECT_FALSE(IsInContainer(*agent1, *container));
  EXPECT_TRUE(IsInContainer(*agent2, *container));

  agent2->Remove();

  EXPECT_EQ(GetAgentCount(*container), 0ul);
  EXPECT_FALSE(IsInContainer(*agent2, *container));
}

// Test that navigating to a new document causes the agents to be disconnected
// from their hosts.
TEST_F(AnnotationAgentContainerImplTest, NavigationRemovesBoundAgents) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest request_next("https://example.com/next.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  MockAnnotationAgentHost host;
  auto remote_receiver_pair = host.BindForCreateAgent();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  container->CreateAgent(std::move(remote_receiver_pair.first),
                         std::move(remote_receiver_pair.second),
                         mojom::blink::AnnotationType::kSharedHighlight,
                         "MockAnnotationSelector");
  ASSERT_EQ(GetAgentCount(*container), 1ul);
  ASSERT_FALSE(host.did_disconnect_);

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");

  host.FlushForTesting();
  EXPECT_TRUE(host.did_disconnect_);
}

// Test that a detached document's container is no longer accessible.
TEST_F(AnnotationAgentContainerImplTest,
       DetachedDocumentContainerBecomesInaccessible) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest request_next("https://example.com/next.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");
  Compositor().BeginFrame();

  auto& first_document = GetDocument();

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");

  EXPECT_FALSE(AnnotationAgentContainerImpl::CreateIfNeeded(first_document));
}

// When the document has no selection, calling CreateAgentFromSelection must
// not create an agent and it must return empty null bindings back to the
// caller.
TEST_F(AnnotationAgentContainerImplTest,
       CreateAgentFromSelectionWithNoSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>TEST PAGE</body>
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  bool did_reply = false;
  container->CreateAgentFromSelection(
      mojom::blink::AnnotationType::kUserNote,
      base::BindLambdaForTesting(
          [&did_reply](
              mojom::blink::SelectorCreationResultPtr selector_creation_result,
              shared_highlighting::LinkGenerationError error,
              shared_highlighting::LinkGenerationReadyStatus ready_status) {
            did_reply = true;

            EXPECT_FALSE(selector_creation_result);
            EXPECT_EQ(
                error,
                shared_highlighting::LinkGenerationError::kEmptySelection);
            // Test that the generation was not preemptive, the result was not
            // ready by the time we called CreateAgentFromSelection.
            EXPECT_EQ(ready_status,
                      shared_highlighting::LinkGenerationReadyStatus::
                          kRequestedBeforeReady);
          }));

  EXPECT_TRUE(did_reply);
  EXPECT_EQ(GetAgentCount(*container), 0ul);
}

// CreateAgentFromSelection must create an agent and return a selector for the
// selected text and bindings to the agent. It should also attach the agent to
// show the highlight.
TEST_F(AnnotationAgentContainerImplTest,
       CreateAgentFromSelectionWithCollapsedSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>TEST PAGE</body>
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  FrameSelection& frame_selection = GetDocument().GetFrame()->Selection();

  Element* body = GetDocument().body();
  frame_selection.SetSelection(SelectionInDOMTree::Builder()
                                   .Collapse(Position(body->firstChild(), 0))
                                   .Build(),
                               SetSelectionOptions());

  bool did_reply = false;
  container->CreateAgentFromSelection(
      mojom::blink::AnnotationType::kUserNote,
      base::BindLambdaForTesting(
          [&did_reply](
              mojom::blink::SelectorCreationResultPtr selector_creation_result,
              shared_highlighting::LinkGenerationError error,
              shared_highlighting::LinkGenerationReadyStatus ready_status) {
            did_reply = true;

            EXPECT_FALSE(selector_creation_result);
            EXPECT_EQ(
                error,
                shared_highlighting::LinkGenerationError::kEmptySelection);
            EXPECT_EQ(ready_status,
                      shared_highlighting::LinkGenerationReadyStatus::
                          kRequestedBeforeReady);
          }));

  EXPECT_TRUE(did_reply);
  EXPECT_EQ(GetAgentCount(*container), 0ul);
}

// CreateAgentFromSelection should synchronously return a preemptively generated
// result if one is available.
TEST_F(AnnotationAgentContainerImplTest,
       CreateAgentFromSelectionWithPreemptiveGeneration) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>TEST PAGE</body>
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  FrameSelection& frame_selection = GetDocument().GetFrame()->Selection();

  Element* body = GetDocument().body();
  EphemeralRange range = EphemeralRange(Position(body->firstChild(), 0),
                                        Position(body->firstChild(), 5));
  ASSERT_EQ("TEST ", PlainText(range));

  frame_selection.SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      SetSelectionOptions());

  // Right click on the selected text
  const auto& selection_rect = CreateRange(range)->BoundingBox();
  SendRightClick(selection_rect.origin());

  MockAnnotationAgentHost host;

  base::RunLoop run_loop;

  run_loop.RunUntilIdle();

  bool did_reply = false;
  container->CreateAgentFromSelection(
      mojom::blink::AnnotationType::kUserNote,
      base::BindLambdaForTesting(
          [&did_reply, &host](
              mojom::blink::SelectorCreationResultPtr selector_creation_result,
              shared_highlighting::LinkGenerationError error,
              shared_highlighting::LinkGenerationReadyStatus ready_status) {
            did_reply = true;

            EXPECT_EQ(selector_creation_result->selected_text, "TEST");
            EXPECT_EQ(selector_creation_result->serialized_selector,
                      "TEST,-PAGE");
            EXPECT_TRUE(selector_creation_result->host_receiver.is_valid());
            EXPECT_TRUE(selector_creation_result->agent_remote.is_valid());
            EXPECT_EQ(error, shared_highlighting::LinkGenerationError::kNone);
            // Test that the generation was preemptive, the result was ready by
            // the time we called CreateAgentFromSelection.
            EXPECT_EQ(ready_status,
                      shared_highlighting::LinkGenerationReadyStatus::
                          kRequestedAfterReady);

            host.Bind(std::move(selector_creation_result->host_receiver),
                      std::move(selector_creation_result->agent_remote));
          }));

  // Test that the callback from CreateAgentFromSelection invoked synchronously.
  EXPECT_TRUE(did_reply);

  EXPECT_TRUE(host.agent_.is_connected());

  // Creating an agent from selection should automatically attach, which will
  // happen in the next BeginFrame.
  Compositor().BeginFrame();
  host.FlushForTesting();
  EXPECT_TRUE(host.did_finish_attachment_rect_);

  EXPECT_EQ(GetAgentCount(*container), 1ul);
}

// When the document has a collapsed selection, calling
// CreateAgentFromSelection must not create an agent and it must return empty
// null bindings back to the caller.
TEST_F(AnnotationAgentContainerImplTest, CreateAgentFromSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>TEST PAGE</body>
  )HTML");
  Compositor().BeginFrame();

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  FrameSelection& frame_selection = GetDocument().GetFrame()->Selection();

  Element* body = GetDocument().body();
  EphemeralRange range = EphemeralRange(Position(body->firstChild(), 0),
                                        Position(body->firstChild(), 5));
  ASSERT_EQ("TEST ", PlainText(range));

  frame_selection.SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      SetSelectionOptions());

  const auto& selection_rect = CreateRange(range)->BoundingBox();
  SendRightClick(selection_rect.origin());

  MockAnnotationAgentHost host;

  base::RunLoop run_loop;
  container->CreateAgentFromSelection(
      mojom::blink::AnnotationType::kUserNote,
      base::BindLambdaForTesting(
          [&run_loop, &host](
              mojom::blink::SelectorCreationResultPtr selector_creation_result,
              shared_highlighting::LinkGenerationError error,
              shared_highlighting::LinkGenerationReadyStatus ready_status) {
            run_loop.Quit();

            EXPECT_EQ(selector_creation_result->selected_text, "TEST");
            EXPECT_EQ(selector_creation_result->serialized_selector,
                      "TEST,-PAGE");
            EXPECT_TRUE(selector_creation_result->host_receiver.is_valid());
            EXPECT_TRUE(selector_creation_result->agent_remote.is_valid());
            EXPECT_EQ(error, shared_highlighting::LinkGenerationError::kNone);
            // Test that the generation was preemptive, the result was ready by
            // the time we called CreateAgentFromSelection.
            EXPECT_EQ(ready_status,
                      shared_highlighting::LinkGenerationReadyStatus::
                          kRequestedAfterReady);

            host.Bind(std::move(selector_creation_result->host_receiver),
                      std::move(selector_creation_result->agent_remote));
          }));
  run_loop.Run();

  EXPECT_TRUE(host.agent_.is_connected());

  // Creating an agent from selection should automatically attach, which will
  // happen in the next BeginFrame.
  Compositor().BeginFrame();
  host.FlushForTesting();
  EXPECT_TRUE(host.did_finish_attachment_rect_);

  EXPECT_EQ(GetAgentCount(*container), 1ul);
}

// Test that an in-progress, asynchronous generation is canceled gracefully if
// a new document is navigated.
TEST_F(AnnotationAgentContainerImplTest, ShutdownDocumentWhileGenerating) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest request_next("https://example.com/next.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body>
      <p>Multiple blocks</p>
      <p>Multiple blocks</p>
      <p>Multiple blocks</p>
      <p>Multiple blocks</p>
      <p>Multiple blocks</p>
      <p id="target">TARGET</p>
      <p>Multiple blocks</p>
    </body>
  )HTML");
  Compositor().BeginFrame();

  // Set a tiny timeout so that the generator takes many tasks to finish its
  // work.
  auto auto_reset_timeout =
      AsyncFindBuffer::OverrideTimeoutForTesting(base::TimeDelta::Min());

  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());

  FrameSelection& frame_selection = GetDocument().GetFrame()->Selection();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EphemeralRange range =
      EphemeralRange(Position(target, 0), Position(target, 1));
  ASSERT_EQ("TARGET", PlainText(range));

  frame_selection.SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build(),
      SetSelectionOptions());

  // Right click on the selected text
  const auto& selection_rect = CreateRange(range)->BoundingBox();
  SendRightClick(selection_rect.origin());

  base::RunLoop run_loop;
  bool did_finish = false;

  container->CreateAgentFromSelection(
      mojom::blink::AnnotationType::kUserNote,
      base::BindLambdaForTesting(
          [&did_finish](
              mojom::blink::SelectorCreationResultPtr selector_creation_result,
              shared_highlighting::LinkGenerationError error,
              shared_highlighting::LinkGenerationReadyStatus ready_status) {
            did_finish = true;
            EXPECT_FALSE(selector_creation_result);
            EXPECT_EQ(
                error,
                shared_highlighting::LinkGenerationError::kIncorrectSelector);
            // Test that the generation was preemptive, the result was ready by
            // the time we called CreateAgentFromSelection.
            EXPECT_EQ(ready_status,
                      shared_highlighting::LinkGenerationReadyStatus::
                          kRequestedAfterReady);
          }));

  // The above will have posted the first generator task to
  // kInternalFindInPage. Post a task after it to exit back to test code after
  // that task runs.
  GetDocument()
      .GetTaskRunner(TaskType::kInternalFindInPage)
      ->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // The generator should still not have completed. Navigate the page to a new
  // document.
  EXPECT_FALSE(did_finish);
  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");

  // The generation should complete but return failure, the agent should not
  // have been created.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_finish);
  EXPECT_EQ(GetAgentCount(*container), 0ul);

  // Ensure the new document doesn't somehow get involved.
  auto* new_container =
      AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  ASSERT_NE(new_container, container);
  EXPECT_EQ(GetAgentCount(*new_container), 0ul);
}

}  // namespace blink
