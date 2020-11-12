// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
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

  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(0),
                                        GURL("data:text/plain,Alpha")));
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root_frame->child_at(1),
      embedded_test_server()->GetURL("/accessibility/snapshot/inner.html")));

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

  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(0),
                                        GURL("data:text/plain,Alpha")));

  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          root_frame->child_at(1)->current_frame_host()));
  EXPECT_TRUE(NavigateToURLFromRenderer(
      inner_contents->GetFrameTree()->root(),
      embedded_test_server()->GetURL("/accessibility/snapshot/inner.html")));

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

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest, SnapshotPDFMode) {
  // The "PDF" accessibility mode is used when getting a snapshot of the
  // accessibility tree in order to export a tagged PDF. Ensure that
  // we're serializing the right set of attributes needed for a PDF and
  // also ensure that we're *not* wasting time serializing attributes
  // that are not needed for PDF export.
  GURL url(R"HTML(data:text/html,<body>
                  <img src="" alt="Unicorns">
                  <ul>
                    <li aria-posinset="5">
                      <span style="color: red;">Red text</span>
                  </ul>
                  <table role="table">
                    <tr>
                      <td colspan="2">
                    </tr>
                    <tr>
                      <td>1</td><td>2</td>
                    </tr>
                  </table>
                  </body>)HTML");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::AXMode::kPDF);
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  // Scan all of the nodes and make some general assertions.
  int dom_node_id_count = 0;
  for (const ui::AXNodeData& node_data : waiter.snapshot().nodes) {
    // Every node should have a valid role, state, and ID.
    EXPECT_NE(ax::mojom::Role::kUnknown, node_data.role);
    EXPECT_NE(0, node_data.id);

    if (node_data.GetIntAttribute(ax::mojom::IntAttribute::kDOMNodeId) != 0)
      dom_node_id_count++;

    // We don't need bounding boxes to make a tagged PDF. Ensure those are
    // uninitialized.
    EXPECT_TRUE(node_data.relative_bounds.bounds.IsEmpty());

    // We shouldn't get any inline text box nodes. They aren't needed to
    // make a tagged PDF and they make up a large fraction of nodes in the
    // tree when present.
    EXPECT_NE(ax::mojom::Role::kInlineTextBox, node_data.role);

    // We shouldn't have any style information like color in the tree.
    EXPECT_FALSE(node_data.HasIntAttribute(ax::mojom::IntAttribute::kColor));
  }

  // Many nodes should have a DOM node id. That's not normally included
  // in the accessibility tree but it's needed for associating nodes with
  // rendered text in the PDF file.
  EXPECT_GT(dom_node_id_count, 5);

  // Build an AXTree from the snapshot and make some specific assertions.
  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  ASSERT_TRUE(root);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, root->data().role);

  // Img alt text should be present.
  ui::AXNode* image = root->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(image);
  ASSERT_EQ(ax::mojom::Role::kImage, image->data().role);
  ASSERT_EQ("Unicorns", image->data().GetStringAttribute(
                            ax::mojom::StringAttribute::kName));

  // List attributes like posinset should be present.
  ui::AXNode* ul = root->GetUnignoredChildAtIndex(1);
  ASSERT_TRUE(ul);
  ASSERT_EQ(ax::mojom::Role::kList, ul->data().role);
  ui::AXNode* li = ul->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(li);
  ASSERT_EQ(ax::mojom::Role::kListItem, li->data().role);
  EXPECT_EQ(5, *li->GetPosInSet());

  // Table attributes like colspan should be present.
  ui::AXNode* table = root->GetUnignoredChildAtIndex(2);
  ASSERT_TRUE(table);
  ASSERT_EQ(ax::mojom::Role::kTable, table->data().role);
  ui::AXNode* tr = table->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(tr);
  ASSERT_EQ(ax::mojom::Role::kRow, tr->data().role);
  ui::AXNode* td = tr->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(td);
  ASSERT_EQ(ax::mojom::Role::kCell, td->data().role);
  EXPECT_EQ(2, *td->GetTableCellColSpan());
}

}  // namespace content
