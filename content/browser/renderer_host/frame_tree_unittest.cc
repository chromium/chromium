// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"

namespace content {

namespace {

// Appends a description of the structure of the frame tree to |result|.
void AppendTreeNodeState(FrameTreeNode* node, std::string* result) {
  result->append(
      base::NumberToString(node->current_frame_host()->GetRoutingID()));
  if (!node->current_frame_host()->IsRenderFrameLive())
    result->append("*");  // Asterisk next to dead frames.

  if (!node->frame_name().empty()) {
    result->append(" '");
    result->append(node->frame_name());
    result->append("'");
  }
  result->append(": [");
  const char* separator = "";
  for (size_t i = 0; i < node->child_count(); i++) {
    result->append(separator);
    AppendTreeNodeState(node->child_at(i), result);
    separator = ", ";
  }
  result->append("]");
}

mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
CreateStubInterfaceProviderReceiver() {
  return TestRenderFrameHost::CreateStubInterfaceProviderReceiver();
}

mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
CreateStubBrowserInterfaceBrokerReceiver() {
  return TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver();
}

// Logs calls to WebContentsObserver along with the state of the frame tree,
// for later use in EXPECT_EQ().
class TreeWalkingWebContentsLogger : public WebContentsObserver {
 public:
  explicit TreeWalkingWebContentsLogger(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ~TreeWalkingWebContentsLogger() override {
    EXPECT_EQ("", log_) << "Activity logged that was not expected";
  }

  // Gets and resets the log, which is a string of what happened.
  std::string GetLog() {
    std::string result = log_;
    log_.clear();
    return result;
  }

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    LogWhatHappened("RenderFrameCreated", render_frame_host);
  }

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    if (old_host)
      LogWhatHappened("RenderFrameHostChanged(old)", old_host);
    LogWhatHappened("RenderFrameHostChanged(new)", new_host);
  }

  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    LogWhatHappened("RenderFrameDeleted", render_frame_host);
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    LogWhatHappened("RenderProcessGone");
  }

 private:
  void LogWhatHappened(const std::string& event_name) {
    if (!log_.empty()) {
      log_.append("\n");
    }
    log_.append(event_name + " -> ");
    AppendTreeNodeState(
        static_cast<WebContentsImpl*>(web_contents())->GetFrameTree()->root(),
        &log_);
  }

  void LogWhatHappened(const std::string& event_name, RenderFrameHost* rfh) {
    LogWhatHappened(
        base::StringPrintf("%s(%d)", event_name.c_str(), rfh->GetRoutingID()));
  }

  std::string log_;

  DISALLOW_COPY_AND_ASSIGN(TreeWalkingWebContentsLogger);
};

}  // namespace

class FrameTreeTest : public RenderViewHostImplTestHarness {
 protected:
  // Prints a FrameTree, for easy assertions of the tree hierarchy.
  std::string GetTreeState(FrameTree* frame_tree) {
    std::string result;
    AppendTreeNodeState(frame_tree->root(), &result);
    return result;
  }

  std::string GetTraversalOrder(FrameTree* frame_tree,
                                FrameTreeNode* subtree_to_skip) {
    std::string result;
    for (FrameTreeNode* node :
         frame_tree->NodesExceptSubtree(subtree_to_skip)) {
      if (!result.empty())
        result += " ";
      result +=
          base::NumberToString(node->current_frame_host()->GetRoutingID());
    }
    return result;
  }
};

// Exercise tree manipulation routines.
//  - Add a series of nodes and verify tree structure.
//  - Remove a series of nodes and verify tree structure.
TEST_F(FrameTreeTest, Shape) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Use the FrameTree of the WebContents so that it has all the delegates it
  // needs.  We may want to consider a test version of this.
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();

  std::string no_children_node("no children node");
  std::string deep_subtree("node with deep subtree");
  int process_id = root->current_frame_host()->GetProcess()->GetID();

  // Do not navigate each frame separately, since that will clutter the test
  // itself. Instead, leave them in "not live" state, which is indicated by the
  // * after the frame id, since this test cares about the shape, not the
  // frame liveness.
  EXPECT_EQ("1: []", GetTreeState(frame_tree));

  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  // Simulate attaching a series of frames to build the frame tree.
  frame_tree->AddFrame(root->current_frame_host(), process_id, 14,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName0", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(root->current_frame_host(), process_id, 15,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName1", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(root->current_frame_host(), process_id, 16,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName2", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(root->child_at(0)->current_frame_host(), process_id, 244,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName3", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(root->child_at(1)->current_frame_host(), process_id, 255,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, no_children_node,
                       "uniqueName4", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(root->child_at(0)->current_frame_host(), process_id, 245,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName5", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);

  EXPECT_EQ(
      "1: [14: [244: [], 245: []], "
      "15: [255 'no children node': []], "
      "16: []]",
      GetTreeState(frame_tree));

  FrameTreeNode* child_16 = root->child_at(2);
  frame_tree->AddFrame(child_16->current_frame_host(), process_id, 264,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName6", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(child_16->current_frame_host(), process_id, 265,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName7", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(child_16->current_frame_host(), process_id, 266,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName8", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(child_16->current_frame_host(), process_id, 267,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, deep_subtree,
                       "uniqueName9", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(child_16->current_frame_host(), process_id, 268,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName10", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);

  FrameTreeNode* child_267 = child_16->child_at(3);
  frame_tree->AddFrame(child_267->current_frame_host(), process_id, 365,
                       CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName11", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(child_267->child_at(0)->current_frame_host(), process_id,
                       455, CreateStubInterfaceProviderReceiver(),
                       CreateStubBrowserInterfaceBrokerReceiver(),
                       blink::mojom::TreeScopeType::kDocument, std::string(),
                       "uniqueName12", false, base::UnguessableToken::Create(),
                       base::UnguessableToken::Create(), blink::FramePolicy(),
                       blink::mojom::FrameOwnerProperties(), false, kOwnerType);
  frame_tree->AddFrame(
      child_267->child_at(0)->child_at(0)->current_frame_host(), process_id,
      555, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName13",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
      kOwnerType);
  frame_tree->AddFrame(
      child_267->child_at(0)->child_at(0)->child_at(0)->current_frame_host(),
      process_id, 655, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName14",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
      kOwnerType);

  // Now that's it's fully built, verify the tree structure is as expected.
  EXPECT_EQ(
      "1: [14: [244: [], 245: []], "
      "15: [255 'no children node': []], "
      "16: [264: [], 265: [], 266: [], "
      "267 'node with deep subtree': "
      "[365: [455: [555: [655: []]]]], 268: []]]",
      GetTreeState(frame_tree));

  // Verify that traversal order is breadth first, even if we skip a subtree.
  FrameTreeNode* child_14 = root->child_at(0);
  FrameTreeNode* child_15 = root->child_at(1);
  FrameTreeNode* child_244 = child_14->child_at(0);
  FrameTreeNode* child_245 = child_14->child_at(1);
  FrameTreeNode* child_555 = child_267->child_at(0)->child_at(0)->child_at(0);
  FrameTreeNode* child_655 = child_555->child_at(0);
  EXPECT_EQ("1 14 15 16 244 245 255 264 265 266 267 268 365 455 555 655",
            GetTraversalOrder(frame_tree, nullptr));
  EXPECT_EQ("1", GetTraversalOrder(frame_tree, root));
  EXPECT_EQ("1 14 15 16 255 264 265 266 267 268 365 455 555 655",
            GetTraversalOrder(frame_tree, child_14));
  EXPECT_EQ("1 14 15 16 244 245 255 264 265 266 267 268 365 455 555 655",
            GetTraversalOrder(frame_tree, child_244));
  EXPECT_EQ("1 14 15 16 244 245 255 264 265 266 267 268 365 455 555 655",
            GetTraversalOrder(frame_tree, child_245));
  EXPECT_EQ("1 14 15 16 244 245 264 265 266 267 268 365 455 555 655",
            GetTraversalOrder(frame_tree, child_15));
  EXPECT_EQ("1 14 15 16 244 245 255 264 265 266 267 268",
            GetTraversalOrder(frame_tree, child_267));
  EXPECT_EQ("1 14 15 16 244 245 255 264 265 266 267 268 365 455 555",
            GetTraversalOrder(frame_tree, child_555));
  EXPECT_EQ("1 14 15 16 244 245 255 264 265 266 267 268 365 455 555 655",
            GetTraversalOrder(frame_tree, child_655));

  frame_tree->RemoveFrame(child_555);
  EXPECT_EQ(
      "1: [14: [244: [], 245: []], "
      "15: [255 'no children node': []], "
      "16: [264: [], 265: [], 266: [], "
      "267 'node with deep subtree': "
      "[365: [455: []]], 268: []]]",
      GetTreeState(frame_tree));

  frame_tree->RemoveFrame(child_16->child_at(1));
  EXPECT_EQ(
      "1: [14: [244: [], 245: []], "
      "15: [255 'no children node': []], "
      "16: [264: [], 266: [], "
      "267 'node with deep subtree': "
      "[365: [455: []]], 268: []]]",
      GetTreeState(frame_tree));

  frame_tree->RemoveFrame(root->child_at(1));
  EXPECT_EQ(
      "1: [14: [244: [], 245: []], "
      "16: [264: [], 266: [], "
      "267 'node with deep subtree': "
      "[365: [455: []]], 268: []]]",
      GetTreeState(frame_tree));
}

// Ensure frames can be found by frame_tree_node_id, routing ID, or name.
TEST_F(FrameTreeTest, FindFrames) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Add a few child frames to the main frame.
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();

  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  main_test_rfh()->OnCreateChildFrame(
      22, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "child0", "uniqueName0", false,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      23, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "child1", "uniqueName1", false,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      24, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName2",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Add one grandchild frame.
  child1->current_frame_host()->OnCreateChildFrame(
      33, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "grandchild", "uniqueName3",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  FrameTreeNode* grandchild = child1->child_at(0);

  // Ensure they can be found by FTN id.
  EXPECT_EQ(root, frame_tree->FindByID(root->frame_tree_node_id()));
  EXPECT_EQ(child0, frame_tree->FindByID(child0->frame_tree_node_id()));
  EXPECT_EQ(child1, frame_tree->FindByID(child1->frame_tree_node_id()));
  EXPECT_EQ(child2, frame_tree->FindByID(child2->frame_tree_node_id()));
  EXPECT_EQ(grandchild, frame_tree->FindByID(grandchild->frame_tree_node_id()));
  EXPECT_EQ(nullptr, frame_tree->FindByID(-1));

  // Ensure they can be found by routing id.
  int process_id = main_test_rfh()->GetProcess()->GetID();
  EXPECT_EQ(root, frame_tree->FindByRoutingID(process_id,
                                              main_test_rfh()->GetRoutingID()));
  EXPECT_EQ(child0, frame_tree->FindByRoutingID(process_id, 22));
  EXPECT_EQ(child1, frame_tree->FindByRoutingID(process_id, 23));
  EXPECT_EQ(child2, frame_tree->FindByRoutingID(process_id, 24));
  EXPECT_EQ(grandchild, frame_tree->FindByRoutingID(process_id, 33));
  EXPECT_EQ(nullptr, frame_tree->FindByRoutingID(process_id, 37));

  // Ensure they can be found by name, if they have one.
  EXPECT_EQ(root, frame_tree->FindByName(std::string()));
  EXPECT_EQ(child0, frame_tree->FindByName("child0"));
  EXPECT_EQ(child1, frame_tree->FindByName("child1"));
  EXPECT_EQ(grandchild, frame_tree->FindByName("grandchild"));
  EXPECT_EQ(nullptr, frame_tree->FindByName("no such frame"));
}

// Check that PreviousSibling() and NextSibling() are retrieved correctly.
TEST_F(FrameTreeTest, GetSibling) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  // Add a few child frames to the main frame.
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  main_test_rfh()->OnCreateChildFrame(
      22, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "child0", "uniqueName0", false,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      23, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "child1", "uniqueName1", false,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      24, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "child2", "uniqueName2", false,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Add one grandchild frame.
  child1->current_frame_host()->OnCreateChildFrame(
      33, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, "grandchild", "uniqueName3",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  FrameTreeNode* grandchild = child1->child_at(0);

  // Test PreviousSibling().
  EXPECT_EQ(nullptr, root->PreviousSibling());
  EXPECT_EQ(nullptr, child0->PreviousSibling());
  EXPECT_EQ(child0, child1->PreviousSibling());
  EXPECT_EQ(child1, child2->PreviousSibling());
  EXPECT_EQ(nullptr, grandchild->PreviousSibling());

  // Test NextSibling().
  EXPECT_EQ(nullptr, root->NextSibling());
  EXPECT_EQ(child1, child0->NextSibling());
  EXPECT_EQ(child2, child1->NextSibling());
  EXPECT_EQ(nullptr, child2->NextSibling());
  EXPECT_EQ(nullptr, grandchild->NextSibling());
}

// Do some simple manipulations of the frame tree, making sure that
// WebContentsObservers see a consistent view of the tree as we go.
TEST_F(FrameTreeTest, ObserverWalksTreeDuringFrameCreation) {
  TreeWalkingWebContentsLogger activity(contents());
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_EQ("RenderFrameCreated(1) -> 1: []", activity.GetLog());

  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();

  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  // Simulate attaching a series of frames to build the frame tree.
  main_test_rfh()->OnCreateChildFrame(
      14, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(14) -> 1: []\n"
      "RenderFrameCreated(14) -> 1: [14: []]",
      activity.GetLog());
  main_test_rfh()->OnCreateChildFrame(
      18, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName1",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(18) -> 1: [14: []]\n"
      "RenderFrameCreated(18) -> 1: [14: [], 18: []]",
      activity.GetLog());
  frame_tree->RemoveFrame(root->child_at(0));
  EXPECT_EQ("RenderFrameDeleted(14) -> 1: [18: []]", activity.GetLog());
  frame_tree->RemoveFrame(root->child_at(0));
  EXPECT_EQ("RenderFrameDeleted(18) -> 1: []", activity.GetLog());
}

// Make sure that WebContentsObservers see a consistent view of the tree after
// recovery from a render process crash.
TEST_F(FrameTreeTest, ObserverWalksTreeAfterCrash) {
  TreeWalkingWebContentsLogger activity(contents());
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_EQ("RenderFrameCreated(1) -> 1: []", activity.GetLog());

  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  main_test_rfh()->OnCreateChildFrame(
      22, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(22) -> 1: []\n"
      "RenderFrameCreated(22) -> 1: [22: []]",
      activity.GetLog());
  main_test_rfh()->OnCreateChildFrame(
      23, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName1",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(23) -> 1: [22: []]\n"
      "RenderFrameCreated(23) -> 1: [22: [], 23: []]",
      activity.GetLog());

  // Crash the renderer
  main_test_rfh()->GetProcess()->SimulateCrash();
  EXPECT_EQ(
      "RenderProcessGone -> 1*: [22*: [], 23*: []]\n"
      "RenderFrameDeleted(23) -> 1*: []\n"
      "RenderFrameDeleted(22) -> 1*: []\n"
      "RenderFrameDeleted(1) -> 1*: []",
      activity.GetLog());
}

// Ensure that frames are not added to the tree, if the process passed in
// is different than the process of the parent node.
TEST_F(FrameTreeTest, FailAddFrameWithWrongProcessId) {
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  int process_id = root->current_frame_host()->GetProcess()->GetID();

  ASSERT_EQ("1: []", GetTreeState(frame_tree));

  // Simulate attaching a frame from mismatched process id.
  ASSERT_FALSE(frame_tree->AddFrame(
      root->current_frame_host(), process_id + 1, 1,
      CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
      blink::mojom::FrameOwnerElementType::kIframe));
  ASSERT_EQ("1: []", GetTreeState(frame_tree));
}

// Ensure that frames removed while a process has crashed are not preserved in
// the global map of id->frame.
TEST_F(FrameTreeTest, ProcessCrashClearsGlobalMap) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Add a couple child frames to the main frame.
  FrameTreeNode* root = contents()->GetFrameTree()->root();

  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  main_test_rfh()->OnCreateChildFrame(
      22, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);
  main_test_rfh()->OnCreateChildFrame(
      23, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName1",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);

  // Add one grandchild frame.
  RenderFrameHostImpl* child1_rfh = root->child_at(0)->current_frame_host();
  child1_rfh->OnCreateChildFrame(
      33, CreateStubInterfaceProviderReceiver(),
      CreateStubBrowserInterfaceBrokerReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName2",
      false, base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), kOwnerType);

  // Ensure they can be found by id.
  int id1 = root->child_at(0)->frame_tree_node_id();
  int id2 = root->child_at(1)->frame_tree_node_id();
  int id3 = root->child_at(0)->child_at(0)->frame_tree_node_id();
  EXPECT_TRUE(FrameTreeNode::GloballyFindByID(id1));
  EXPECT_TRUE(FrameTreeNode::GloballyFindByID(id2));
  EXPECT_TRUE(FrameTreeNode::GloballyFindByID(id3));

  // Crash the renderer.
  main_test_rfh()->GetProcess()->SimulateCrash();

  // Ensure they cannot be found by id after the process has crashed.
  EXPECT_FALSE(FrameTreeNode::GloballyFindByID(id1));
  EXPECT_FALSE(FrameTreeNode::GloballyFindByID(id2));
  EXPECT_FALSE(FrameTreeNode::GloballyFindByID(id3));
}

}  // namespace content
