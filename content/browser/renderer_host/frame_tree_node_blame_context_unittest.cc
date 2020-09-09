// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree_node_blame_context.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"

namespace content {

namespace {

bool EventPointerCompare(const trace_analyzer::TraceEvent* lhs,
                         const trace_analyzer::TraceEvent* rhs) {
  CHECK(lhs);
  CHECK(rhs);
  return *lhs < *rhs;
}

void ExpectFrameTreeNodeObject(const trace_analyzer::TraceEvent* event) {
  EXPECT_EQ("navigation", event->category);
  EXPECT_EQ("FrameTreeNode", event->name);
}

void ExpectFrameTreeNodeSnapshot(const trace_analyzer::TraceEvent* event) {
  ExpectFrameTreeNodeObject(event);
  EXPECT_TRUE(event->HasArg("snapshot"));
  EXPECT_TRUE(event->arg_values.at("snapshot")->is_dict());
}

std::string GetParentNodeID(const trace_analyzer::TraceEvent* event) {
  const base::Value* arg_snapshot = event->arg_values.at("snapshot").get();
  const base::DictionaryValue* snapshot;
  EXPECT_TRUE(arg_snapshot->GetAsDictionary(&snapshot));
  if (!snapshot->HasKey("parent"))
    return std::string();
  const base::DictionaryValue* parent;
  EXPECT_TRUE(snapshot->GetDictionary("parent", &parent));
  std::string parent_id;
  EXPECT_TRUE(parent->GetString("id_ref", &parent_id));
  return parent_id;
}

std::string GetSnapshotURL(const trace_analyzer::TraceEvent* event) {
  const base::Value* arg_snapshot = event->arg_values.at("snapshot").get();
  const base::DictionaryValue* snapshot;
  EXPECT_TRUE(arg_snapshot->GetAsDictionary(&snapshot));
  if (!snapshot->HasKey("url"))
    return std::string();
  std::string url;
  EXPECT_TRUE(snapshot->GetString("url", &url));
  return url;
}

}  // namespace

class FrameTreeNodeBlameContextTest : public RenderViewHostImplTestHarness {
 public:
  FrameTree* tree() { return contents()->GetFrameTree(); }
  FrameTreeNode* root() { return tree()->root(); }
  int process_id() {
    return root()->current_frame_host()->GetProcess()->GetID();
  }

  // Creates a frame tree specified by |shape|, which is a string of paired
  // parentheses. Each pair of parentheses represents a FrameTreeNode, and the
  // nesting of parentheses represents the parent-child relation between nodes.
  // Nodes represented by outer-most parentheses are children of the root node.
  // NOTE: Each node can have at most 9 child nodes, and the tree height (i.e.,
  // max # of edges in any root-to-leaf path) must be at most 9.
  // See the test cases for sample usage.
  void CreateFrameTree(const char* shape) {
    main_test_rfh()->InitializeRenderFrameIfNeeded();
    CreateSubframes(root(), 1, shape);
  }

  void RemoveAllNonRootFrames() {
    while (root()->child_count())
      tree()->RemoveFrame(root()->child_at(0));
  }

 private:
  int CreateSubframes(FrameTreeNode* node, int self_id, const char* shape) {
    int consumption = 0;
    for (int child_num = 1; shape[consumption++] == '('; ++child_num) {
      int child_id = self_id * 10 + child_num;
      tree()->AddFrame(
          node->current_frame_host(), process_id(), child_id,
          TestRenderFrameHost::CreateStubInterfaceProviderReceiver(),
          TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
          blink::mojom::TreeScopeType::kDocument, std::string(),
          base::StringPrintf("uniqueName%d", child_id), false,
          base::UnguessableToken::Create(), base::UnguessableToken::Create(),
          blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
          blink::mojom::FrameOwnerElementType::kIframe);
      FrameTreeNode* child = node->child_at(child_num - 1);
      consumption += CreateSubframes(child, child_id, shape + consumption);
    }
    return consumption;
  }
};

// Creates a frame tree, tests if (i) the creation of each new frame is
// correctly traced, and (ii) the topology given by the snapshots is correct.
TEST_F(FrameTreeNodeBlameContextTest, FrameCreation) {
  /* Shape of the frame tree to be created:
   *        ()
   *      /    \
   *     ()    ()
   *    /  \   |
   *   ()  ()  ()
   *           |
   *           ()
   */
  const char* tree_shape = "(()())((()))";

  trace_analyzer::Start("*");
  CreateFrameTree(tree_shape);
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  trace_analyzer::Query q =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_CREATE_OBJECT) ||
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_SNAPSHOT_OBJECT);
  analyzer->FindEvents(q, &events);

  // Two events for each new node: creation and snapshot.
  EXPECT_EQ(12u, events.size());

  std::set<FrameTreeNode*> creation_traced;
  std::set<FrameTreeNode*> snapshot_traced;
  for (auto* event : events) {
    ExpectFrameTreeNodeObject(event);
    FrameTreeNode* node =
        tree()->FindByID(strtol(event->id.c_str(), nullptr, 16));
    EXPECT_NE(nullptr, node);
    if (event->HasArg("snapshot")) {
      ExpectFrameTreeNodeSnapshot(event);
      EXPECT_FALSE(base::Contains(snapshot_traced, node));
      snapshot_traced.insert(node);
      std::string parent_id = GetParentNodeID(event);
      EXPECT_FALSE(parent_id.empty());
      EXPECT_EQ(node->parent()->frame_tree_node(),
                tree()->FindByID(strtol(parent_id.c_str(), nullptr, 16)));
    } else {
      EXPECT_EQ(TRACE_EVENT_PHASE_CREATE_OBJECT, event->phase);
      EXPECT_FALSE(base::Contains(creation_traced, node));
      creation_traced.insert(node);
    }
  }
}

// Deletes frames from a frame tree, tests if the destruction of each frame is
// correctly traced.
TEST_F(FrameTreeNodeBlameContextTest, FrameDeletion) {
  /* Shape of the frame tree to be created:
   *        ()
   *      /    \
   *     ()    ()
   *    /  \   |
   *   ()  ()  ()
   *           |
   *           ()
   */
  const char* tree_shape = "(()())((()))";

  CreateFrameTree(tree_shape);
  std::set<int> node_ids;
  for (FrameTreeNode* node : tree()->Nodes())
    node_ids.insert(node->frame_tree_node_id());

  trace_analyzer::Start("*");
  RemoveAllNonRootFrames();
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  trace_analyzer::Query q =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_DELETE_OBJECT);
  analyzer->FindEvents(q, &events);

  // The removal of all non-root nodes should be traced.
  EXPECT_EQ(6u, events.size());
  for (auto* event : events) {
    ExpectFrameTreeNodeObject(event);
    int id = strtol(event->id.c_str(), nullptr, 16);
    EXPECT_TRUE(base::Contains(node_ids, id));
    node_ids.erase(id);
  }
}

// Changes URL of the root node. Tests if URL change is correctly traced.
TEST_F(FrameTreeNodeBlameContextTest, URLChange) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  GURL url1("http://a.com/");
  GURL url2("https://b.net/");

  trace_analyzer::Start("*");
  root()->SetCurrentURL(url1);
  root()->SetCurrentURL(url2);
  root()->SetCurrentURL(GURL());
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  trace_analyzer::Query q =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_SNAPSHOT_OBJECT);
  analyzer->FindEvents(q, &events);
  std::sort(events.begin(), events.end(), EventPointerCompare);

  // Three snapshots are traced, one for each URL change.
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ(url1.spec(), GetSnapshotURL(events[0]));
  EXPECT_EQ(url2.spec(), GetSnapshotURL(events[1]));
  EXPECT_EQ("", GetSnapshotURL(events[2]));
}

}  // namespace content
