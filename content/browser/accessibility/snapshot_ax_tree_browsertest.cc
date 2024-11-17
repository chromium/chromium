// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

namespace {

class AXTreeSnapshotWaiter {
 public:
  AXTreeSnapshotWaiter() : loop_runner_(new MessageLoopRunner()) {}

  AXTreeSnapshotWaiter(const AXTreeSnapshotWaiter&) = delete;
  AXTreeSnapshotWaiter& operator=(const AXTreeSnapshotWaiter&) = delete;

  void Wait() { loop_runner_->Run(); }

  const ui::AXTreeUpdate& snapshot() const { return snapshot_; }

  void ReceiveSnapshot(ui::AXTreeUpdate& snapshot) {
    snapshot_ = snapshot;
    loop_runner_->Quit();
  }

 private:
  ui::AXTreeUpdate snapshot_;
  scoped_refptr<MessageLoopRunner> loop_runner_;
};

void DumpRolesAndNamesAsText(const ui::AXNode* node,
                             int indent,
                             std::string* dst) {
  for (int i = 0; i < indent; i++) {
    *dst += "  ";
  }
  *dst += ui::ToString(node->GetRole());
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    *dst += " '" + node->GetStringAttribute(ax::mojom::StringAttribute::kName) +
            "'";
  }
  *dst += "\n";
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    DumpRolesAndNamesAsText(iter.get(), indent + 1, dst);
  }
}

}  // namespace

class SnapshotAXTreeBrowserTest : public ContentBrowserTest {
 public:
  SnapshotAXTreeBrowserTest() {
#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitAndDisableFeature(
        features::kAccessibilitySnapshotStressTests);
#endif
  }
  ~SnapshotAXTreeBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
      ui::kAXModeComplete,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  ui::AXTree tree(waiter.snapshot());
  ui::AXNode* root = tree.root();
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());
  ui::AXNode* group = root->GetUnignoredChildAtIndex(0);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, group->GetRole());
  ui::AXNode* button = group->GetUnignoredChildAtIndex(0);
  ASSERT_EQ(ax::mojom::Role::kButton, button->GetRole());
}

class SnapshotAXTreeFencedFrameBrowserTest : public SnapshotAXTreeBrowserTest {
 public:
  SnapshotAXTreeFencedFrameBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}}},
        {/* disabled_features */});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SnapshotAXTreeBrowserTest::SetUpOnMainThread();

    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeFencedFrameBrowserTest,
                       SnapshotAccessibilityTreeFromMultipleFrames) {
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", "/fenced_frames/basic.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  RenderFrameHostImpl* primary_rfh = web_contents->GetPrimaryMainFrame();
  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  EXPECT_EQ(1u, fenced_frames.size());

  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  EXPECT_TRUE(
      ExecJs(primary_rfh, JsReplace("document.querySelector('fencedframe')."
                                    "config = new FencedFrameConfig($1);",
                                    fenced_frame_url.spec())));
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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
      "    iframe\n"
      "      rootWebArea\n"
      "        genericContainer\n"
      "          staticText 'This page has no title.'\n",
      dump);
}

IN_PROC_BROWSER_TEST_F(
    SnapshotAXTreeFencedFrameBrowserTest,
    SnapshotAccessibilityTreeFromMultipleFramesLimitedToNonFencedFrames) {
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server()->GetURL("a.test", "/fenced_frames/basic.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  RenderFrameHostImpl* primary_rfh = web_contents->GetPrimaryMainFrame();
  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  EXPECT_EQ(1u, fenced_frames.size());

  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  EXPECT_TRUE(
      ExecJs(primary_rfh, JsReplace("document.querySelector('fencedframe')."
                                    "config = new FencedFrameConfig($1);",
                                    fenced_frame_url.spec())));
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete,
      /* max_nodes= */ 0,
      /* timeout= */ {},
      WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
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
      "    iframe\n",
      dump);
}

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest,
                       SnapshotAccessibilityTreeFromMultipleFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/accessibility/snapshot/outer.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_frame = web_contents->GetPrimaryFrameTree().root();

  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(0),
                                        GURL("data:text/plain,Alpha")));
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root_frame->child_at(1),
      embedded_test_server()->GetURL("/accessibility/snapshot/inner.html")));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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
      "        genericContainer\n"
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

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeFencedFrameBrowserTest,
                       SnapshotAccessibilityTreeFromSameOriginOnly) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Construct an A1(B1(A2),A3,C1) hierarchy.
  GURL url_a1 = embedded_test_server()->GetURL(
      "a.com", "/accessibility/snapshot/hierarchy/a1.html");
  GURL url_a2 = embedded_test_server()->GetURL(
      "a.com", "/accessibility/snapshot/hierarchy/a2.html");
  GURL url_a3 = embedded_test_server()->GetURL(
      "a.com", "/accessibility/snapshot/hierarchy/a3.html");
  GURL url_b1 = embedded_test_server()->GetURL(
      "b.com", "/accessibility/snapshot/hierarchy/b1.html");
  GURL url_c1 = embedded_test_server()->GetURL(
      "c.com", "/accessibility/snapshot/hierarchy/c1.html");

  ASSERT_TRUE(NavigateToURL(shell(), url_a1));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_frame = web_contents->GetPrimaryFrameTree().root();

  ASSERT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(0), url_b1));
  ASSERT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(1), url_a3));
  ASSERT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(2), url_c1));

  ASSERT_TRUE(
      NavigateToURLFromRenderer(root_frame->child_at(0)->child_at(0), url_a2));

  // Get with WebContents::AXTreeSnapshotPolicy::kAll
  {
    AXTreeSnapshotWaiter waiter;
    web_contents->RequestAXTreeSnapshot(
        base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                       base::Unretained(&waiter)),
        ui::kAXModeComplete,
        /* max_nodes= */ 0,
        /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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
        "    staticText 'This is A1'\n"
        "  staticText 'iframe1: '\n"
        "  iframe\n"
        "    rootWebArea\n"
        "      genericContainer\n"
        "        staticText 'This is B1'\n"
        "      staticText 'iframe1: '\n"
        "      iframe\n"
        "        rootWebArea\n"
        "          genericContainer\n"
        "            staticText 'This is A2'\n"
        "  staticText 'iframe2: '\n"
        "  iframe\n"
        "    rootWebArea\n"
        "      genericContainer\n"
        "        staticText 'This is A3'\n"
        "  staticText 'iframe3: '\n"
        "  iframe\n"
        "    rootWebArea\n"
        "      genericContainer\n"
        "        staticText 'This is C1'\n",
        dump);
  }

  // Get with WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants
  {
    AXTreeSnapshotWaiter waiter;
    web_contents->RequestAXTreeSnapshot(
        base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                       base::Unretained(&waiter)),
        ui::kAXModeComplete,
        /* max_nodes= */ 0,
        /* timeout= */ {},
        WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
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
        "    staticText 'This is A1'\n"
        "  staticText 'iframe1: '\n"
        "  iframe\n"
        "  staticText 'iframe2: '\n"
        "  iframe\n"
        "    rootWebArea\n"
        "      genericContainer\n"
        "        staticText 'This is A3'\n"
        "  staticText 'iframe3: '\n"
        "  iframe\n",
        dump);
  }
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
  FrameTreeNode* root_frame = web_contents->GetPrimaryFrameTree().root();

  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame->child_at(0),
                                        GURL("data:text/plain,Alpha")));

  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          root_frame->child_at(1)->current_frame_host()));
  EXPECT_TRUE(NavigateToURLFromRenderer(
      inner_contents->GetPrimaryFrameTree().root(),
      embedded_test_server()->GetURL("/accessibility/snapshot/inner.html")));

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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
      "        genericContainer\n"
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
      ui::kAXModeComplete,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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
      ui::AXMode::kWebContents,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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
  int num_attributes_for_all_contents_nodes = 0;
  int num_attributes_for_all_complete_nodes = 0;
  for (size_t i = 0; i < complete_nodes.size(); ++i) {
    num_attributes_for_all_contents_nodes +=
        total_attribute_count(contents_nodes[i]);
    num_attributes_for_all_complete_nodes +=
        total_attribute_count(complete_nodes[i]);
    EXPECT_LE(total_attribute_count(contents_nodes[i]),
              total_attribute_count(complete_nodes[i]))
        << "\nComplete node should have had more attributes:"
        << "\n* AXNodeData with AXMode=kWebContents: "
        << contents_nodes[i].ToString()
        << "\n* AXNodeData with AXMode=kAXModeComplete: "
        << complete_nodes[i].ToString();
  }
  EXPECT_LT(num_attributes_for_all_contents_nodes,
            num_attributes_for_all_complete_nodes);
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
      ui::AXMode::kPDFPrinting,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
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

    if (node_data.GetDOMNodeId()) {
      dom_node_id_count++;
    }

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
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());

  // Img alt text should be present.
  ui::AXNode* image = root->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(image);
  ASSERT_EQ(ax::mojom::Role::kImage, image->GetRole());
  ASSERT_EQ("Unicorns",
            image->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // List attributes like posinset should be present.
  ui::AXNode* ul = root->GetUnignoredChildAtIndex(1);
  ASSERT_TRUE(ul);
  ASSERT_EQ(ax::mojom::Role::kList, ul->GetRole());
  ui::AXNode* li = ul->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(li);
  ASSERT_EQ(ax::mojom::Role::kListItem, li->GetRole());
  EXPECT_EQ(5, *li->GetPosInSet());

  // Table attributes like colspan should be present.
  ui::AXNode* table = root->GetUnignoredChildAtIndex(2);
  ASSERT_TRUE(table);
  ASSERT_EQ(ax::mojom::Role::kTable, table->GetRole());
  ui::AXNode* tr = table->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(tr);
  ASSERT_EQ(ax::mojom::Role::kRow, tr->GetRole());
  ui::AXNode* td = tr->GetUnignoredChildAtIndex(0);
  ASSERT_TRUE(td);
  ASSERT_EQ(ax::mojom::Role::kCell, td->GetRole());
  EXPECT_EQ(2, *td->GetTableCellColSpan());
}

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest, MaxNodes) {
  GURL url(R"HTML(data:text/html,<body>
                  <style> p { margin: 50px; } </style>
                  <script>
                    for (let i = 0; i < 10; i++) {
                      let div = document.createElement('div');
                      for (let j = 0; j < 10; j++) {
                        let p = document.createElement('p');
                        p.innerHTML = i;
                        div.appendChild(p);
                      }
                      document.body.appendChild(div);
                    }
                  </script>
                  </body>)HTML");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete,
      /* max_nodes= */ 10,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
  waiter.Wait();

  // Dump the whole tree if one of the assertions below fails
  // to aid in debugging why it failed.
  SCOPED_TRACE(waiter.snapshot().ToString());

  // If we didn't set a maximum number of nodes, thee would be at least 200
  // nodes on the page (2 for every paragraph, and there are 10 divs each
  // containing 10 paragraphs). By setting the max to 10 nodes, we should
  // get only the first div - and the rest of the divs will be empty.
  // The end result is a little more than 20 nodes, nowhere close to 200.
  EXPECT_LT(waiter.snapshot().nodes.size(), 35U);
}

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest, Timeout) {
  GURL url(R"HTML(data:text/html,<body>
                  <style> p { margin: 50px; } </style>
                  <script>
                    for (let i = 0; i < 100; i++) {
                      let p = document.createElement('p');
                      p.innerHTML = i;
                      document.body.append(p);
                    }
                  </script>
                  </body>)HTML");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // Get the number of nodes with no timeout.
  size_t actual_nodes = 0;
  {
    AXTreeSnapshotWaiter waiter;
    web_contents->RequestAXTreeSnapshot(
        base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                       base::Unretained(&waiter)),
        ui::kAXModeComplete,
        /* max_nodes= */ 0,
        /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
    waiter.Wait();
    actual_nodes = waiter.snapshot().nodes.size();
    LOG(INFO) << "Actual nodes: " << actual_nodes;
  }

  // Request a snapshot with a timeout of 1 ms. The test succeeds if
  // we get fewer nodes. There's a tiny chance we don't hit the timeout,
  // so keep trying indefinitely until the test either passes or times out.
  size_t nodes_with_timeout = actual_nodes;
  while (nodes_with_timeout >= actual_nodes) {
    AXTreeSnapshotWaiter waiter;
    web_contents->RequestAXTreeSnapshot(
        base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                       base::Unretained(&waiter)),
        ui::kAXModeComplete,
        /* max_nodes= */ 0,
        /* timeout= */ base::Milliseconds(1),
        WebContents::AXTreeSnapshotPolicy::kAll);
    waiter.Wait();

    nodes_with_timeout = waiter.snapshot().nodes.size();
    LOG(INFO) << "Nodes with timeout: " << nodes_with_timeout;
  }

  EXPECT_LT(nodes_with_timeout, actual_nodes);
}

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeBrowserTest, Metadata) {
  GURL url(R"HTML(data:text/html,
                  <head>
                    <title>Hello World</title>
                    <script>console.log("Skip me!");</script>
                    <meta charset="utf-8">
                    <link ref="canonical" href="https://abc.com">
                    <script type="application/ld+json">{}</script>
                  </head>
                  <body>
                    Hello, world!
                  </body>)HTML");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  ui::AXMode mode(ui::AXMode::kWebContents | ui::AXMode::kHTML |
                  ui::AXMode::kHTMLMetadata);

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      mode,
      /* max_nodes= */ 0,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
  waiter.Wait();

  EXPECT_THAT(
      waiter.snapshot().tree_data.metadata,
      testing::ElementsAre(
          "<title>Hello World</title>", "<meta charset=\"utf-8\"></meta>",
          "<link ref=\"canonical\" href=\"https://abc.com\"></link>",
          "<script type=\"application/ld+json\">{}</script>"));
}

// For Android, test the field trial param to change the number of max nodes.
#if BUILDFLAG(IS_ANDROID)
class SnapshotAXTreeMaxNodesParamBrowserTest : public ContentBrowserTest {
 public:
  SnapshotAXTreeMaxNodesParamBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAccessibilitySnapshotStressTests,
        {{"AccessibilitySnapshotStressTestsMaxNodes", "500"}});
  }
  ~SnapshotAXTreeMaxNodesParamBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SnapshotAXTreeMaxNodesParamBrowserTest, MaxNodes) {
  GURL url(R"HTML(data:text/html,<body>
                  <style> p { margin: 50px; } </style>
                  <script>
                    let outerDiv = document.createElement('div');
                    for (let i = 0; i < 200; i++) {
                      let div = document.createElement('div');
                      for (let j = 0; j < 20; j++) {
                        let p = document.createElement('p');
                        p.innerHTML = j;
                        div.appendChild(p);
                      }
                      outerDiv.appendChild(div);
                    }
                    document.body.appendChild(outerDiv);
                  </script>
                  </body>)HTML");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  AXTreeSnapshotWaiter waiter;
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(&AXTreeSnapshotWaiter::ReceiveSnapshot,
                     base::Unretained(&waiter)),
      ui::kAXModeComplete,
      /* max_nodes= */ 10,
      /* timeout= */ {}, WebContents::AXTreeSnapshotPolicy::kAll);
  waiter.Wait();

  // If the feature flag and param was honored, we should see more than 100
  // nodes (which was the value set on the method call), but less than all the
  // possible nodes.
  EXPECT_GT(waiter.snapshot().nodes.size(), 100U);
  EXPECT_LT(waiter.snapshot().nodes.size(), 800U);
}

#endif
}  // namespace content
