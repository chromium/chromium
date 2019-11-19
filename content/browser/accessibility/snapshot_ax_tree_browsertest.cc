// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

namespace {

class AXTreeSnapshotWaiter {
 public:
  AXTreeSnapshotWaiter() : loop_runner_(new MessageLoopRunner()) {}

  void Wait() { loop_runner_->Run(); }

  const ui::AXTreeUpdate& snapshot() const { return snapshot_; }

  void ReceiveSnapshot(const ui::AXTreeUpdate& snapshot) {
    snapshot_ = snapshot;
    loop_runner_->Quit();
  }

 private:
  ui::AXTreeUpdate snapshot_;
  scoped_refptr<MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSnapshotWaiter);
};

void DumpRolesAndNamesAsText(const ui::AXNode* node,
                             int indent,
                             std::string* dst) {
  for (int i = 0; i < indent; i++)
    *dst += "  ";
  *dst += ui::ToString(node->data().role);
  if (node->data().HasStringAttribute(ax::mojom::StringAttribute::kName))
    *dst += " '" +
            node->data().GetStringAttribute(ax::mojom::StringAttribute::kName) +
            "'";
  *dst += "\n";
  for (size_t i = 0; i < node->GetUnignoredChildCount(); ++i)
    DumpRolesAndNamesAsText(node->GetUnignoredChildAtIndex(i), indent + 1, dst);
}

}  // namespace

class SnapshotAXTreeBrowserTest : public ContentBrowserTest {
 public:
  SnapshotAXTreeBrowserTest() {}
  ~SnapshotAXTreeBrowserTest() override {}
};

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeFromWebContents) {
  GURL url("data:text/html,<button>Click</button>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete);
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, root->data().role);
  ui::AXNode* group = root->GetUnignoredChildAtIndex(0);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, group->data().role);
  ui::AXNode* button = group->GetUnignoredChildAtIndex(0);
  ASSERT_EQ(ax::mojom::Role::kButton, button->data().role);
}

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeFromMultipleFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/accessibility/snapshot/outer.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_frame = web_contents->GetFrameTree()->root();

  NavigateFrameToURL(root_frame->child_at(0), GURL("data:text/plain,Alpha"));
  NavigateFrameToURL(
      root_frame->child_at(1),
      embedded_test_server()->GetURL("/accessibility/snapshot/inner.html"));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete);
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  std::string dump;
  DumpRolesAndNamesAsText(root, 0, &dump);
  EXPECT_EQ(
      "rootWebArea\n"
      "  genericContainer\n"
      "    button 'Before'\n"
      "      staticText 'Before'\n"
      "    iframe\n"
      "      rootWebArea\n"
      "        pre\n"
      "          staticText 'Alpha'\n"
      "    button 'Middle'\n"
      "      staticText 'Middle'\n"
      "    iframe\n"
      "      rootWebArea\n"
      "        genericContainer\n"
      "          button 'Inside Before'\n"
      "            staticText 'Inside Before'\n"
      "          iframe\n"
      "            rootWebArea\n"
      "          button 'Inside After'\n"
      "            staticText 'Inside After'\n"
      "    button 'After'\n"
      "      staticText 'After'\n",
      dump);
}

// This tests to make sure that RequestAXTreeSnapshot() correctly traverses
// inner contents, as used in features like <webview>.
IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeWithInnerContents) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/accessibility/snapshot/outer.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_frame = web_contents->GetFrameTree()->root();

  NavigateFrameToURL(root_frame->child_at(0), GURL("data:text/plain,Alpha"));

  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          root_frame->child_at(1)->current_frame_host()));
  NavigateFrameToURL(
      inner_contents->GetFrameTree()->root(),
      embedded_test_server()->GetURL("/accessibility/snapshot/inner.html"));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete);
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  std::string dump;
  DumpRolesAndNamesAsText(root, 0, &dump);
  EXPECT_EQ(
      "rootWebArea\n"
      "  genericContainer\n"
      "    button 'Before'\n"
      "      staticText 'Before'\n"
      "    iframe\n"
      "      rootWebArea\n"
      "        pre\n"
      "          staticText 'Alpha'\n"
      "    button 'Middle'\n"
      "      staticText 'Middle'\n"
      "    iframe\n"
      "      rootWebArea\n"
      "        genericContainer\n"
      "          button 'Inside Before'\n"
      "            staticText 'Inside Before'\n"
      "          iframe\n"
      "            rootWebArea\n"
      "          button 'Inside After'\n"
      "            staticText 'Inside After'\n"
      "    button 'After'\n"
      "      staticText 'After'\n",
      dump);
}

// This tests to make sure that snapshotting with different modes gives
// different results. This is not intended to ensure that specific modes give
// specific attributes, but merely to ensure that the mode parameter makes a
// difference.
IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeModes) {
  GURL url("data:text/html,<button>Click</button>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  AXTreeSnapshotWaiter waiter_complete;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter_complete)),
      ui::kAXModeComplete);
  waiter_complete.Wait();
  const std::vector<ui::AXNodeData>& complete_nodes =
      waiter_complete.snapshot().nodes;

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter_complete.snapshot().ToString());

  AXTreeSnapshotWaiter waiter_contents;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter_contents)),
      ui::AXMode::kWebContents);
  waiter_contents.Wait();
  const std::vector<ui::AXNodeData>& contents_nodes =
      waiter_contents.snapshot().nodes;

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter_contents.snapshot().ToString());

  // The two snapshot passes walked the tree in the same order, so comparing
  // element to element is possible by walking the snapshots in parallel.

  auto total_attribute_count = [](const ui::AXNodeData& node_data) {
    return node_data.string_attributes.size() +
           node_data.int_attributes.size() + node_data.float_attributes.size() +
           node_data.bool_attributes.size() +
           node_data.intlist_attributes.size() +
           node_data.stringlist_attributes.size() +
           node_data.html_attributes.size();
  };

  ASSERT_EQ(complete_nodes.size(), contents_nodes.size());
  for (size_t i = 0; i < complete_nodes.size(); ++i)
    EXPECT_LT(total_attribute_count(contents_nodes[i]),
              total_attribute_count(complete_nodes[i]));
}

}  // namespace content
