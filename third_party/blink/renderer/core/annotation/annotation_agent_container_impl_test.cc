// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"

#include <gtest/gtest.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_test_utils.h"
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
    return container.agents_.find(&agent) != container.agents_.end();
  }

  size_t GetAgentCount(AnnotationAgentContainerImpl& container) {
    return container.agents_.size();
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

  ASSERT_TRUE(GetDocument().GetFrame());
  ASSERT_TRUE(GetDocument().GetFrame()->FirstChild());
  LocalFrame* child_frame =
      DynamicTo<LocalFrame>(GetDocument().GetFrame()->FirstChild());
  ASSERT_TRUE(child_frame);

  Document* child_document = child_frame->GetDocument();
  ASSERT_TRUE(child_document);

  // Initially, the container supplement should not exist on either document.
  EXPECT_FALSE(
      Supplement<Document>::From<AnnotationAgentContainerImpl>(GetDocument()));
  EXPECT_FALSE(
      Supplement<Document>::From<AnnotationAgentContainerImpl>(child_document));

  // Calling the getter on the container should create the supplement but only
  // for the child document.
  auto* child_container = AnnotationAgentContainerImpl::From(*child_document);
  EXPECT_TRUE(child_container);
  EXPECT_EQ(
      child_container,
      Supplement<Document>::From<AnnotationAgentContainerImpl>(child_document));
  EXPECT_FALSE(
      Supplement<Document>::From<AnnotationAgentContainerImpl>(GetDocument()));

  // Calling the getter for the main document should now create that supplement.
  auto* main_container = AnnotationAgentContainerImpl::From(GetDocument());
  EXPECT_EQ(
      main_container,
      Supplement<Document>::From<AnnotationAgentContainerImpl>(GetDocument()));

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

  mojo::Remote<mojom::blink::AnnotationAgentContainer> remote;
  ASSERT_FALSE(remote.is_bound());
  ASSERT_FALSE(
      Supplement<Document>::From<AnnotationAgentContainerImpl>(GetDocument()));

  AnnotationAgentContainerImpl::BindReceiver(
      GetDocument().GetFrame(), remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(remote.is_connected());
  EXPECT_TRUE(
      Supplement<Document>::From<AnnotationAgentContainerImpl>(GetDocument()));
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

  mojo::Remote<mojom::blink::AnnotationAgentContainer> remote;
  AnnotationAgentContainerImpl::BindReceiver(
      GetDocument().GetFrame(), remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote.is_connected());

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");

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

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");

  auto* container_next = AnnotationAgentContainerImpl::From(GetDocument());

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

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
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

  MockAnnotationAgentHost host;
  auto remote_receiver_pair = host.BindForCreateAgent();

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
  ASSERT_TRUE(container);
  container->CreateAgent(std::move(remote_receiver_pair.first),
                         std::move(remote_receiver_pair.second),
                         mojom::blink::AnnotationType::kSharedHighlight,
                         "MockAnnotationSelector");

  EXPECT_EQ(GetAgentCount(*container), 1ul);

  EXPECT_TRUE(host.agent_.is_connected());

  host.FlushForTesting();

  // Creating a bound agent should automatically start attachment.
  EXPECT_TRUE(host.did_finish_attachment_rect_);
  EXPECT_FALSE(host.did_disconnect_);
}

// Test that an agent removing itself also removes it from its container.
TEST_F(AnnotationAgentContainerImplTest, ManuallyRemoveAgent) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    TEST PAGE
  )HTML");

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
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

  MockAnnotationAgentHost host;
  auto remote_receiver_pair = host.BindForCreateAgent();

  auto* container = AnnotationAgentContainerImpl::From(GetDocument());
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

  auto& first_document = GetDocument();

  LoadURL("https://example.com/next.html");
  request_next.Complete(R"HTML(
    <!DOCTYPE html>
    NEXT PAGE
  )HTML");

  EXPECT_FALSE(AnnotationAgentContainerImpl::From(first_document));
}

}  // namespace blink
