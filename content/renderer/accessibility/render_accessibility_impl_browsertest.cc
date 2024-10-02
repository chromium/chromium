// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/renderer/plugin_ax_tree_action_target_adapter.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/render_accessibility_impl_test.h"
#include "content/renderer/render_frame_impl.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/null_ax_action_target.h"
#include "ui/native_theme/native_theme_features.h"

namespace content {

using blink::WebAXObject;
using blink::WebDocument;

TEST_F(RenderAccessibilityImplTest, SendFullAccessibilityTreeOnReload) {
  // The job of RenderAccessibilityImpl is to serialize the
  // accessibility tree built by WebKit and send it to the browser.
  // When the accessibility tree changes, it tries to send only
  // the nodes that actually changed or were reparented. This test
  // ensures that the messages sent are correct in cases when a page
  // reloads, and that internal state is properly garbage-collected.
  constexpr char html[] = R"HTML(
      <body>
        <div role="group" id="A">
          <div role="group" id="A1"></div>
          <div role="group" id="A2"></div>
        </div>
      </body>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());

  // If we post another event but the tree doesn't change,
  // we should only send 1 node to the browser.
  ClearHandledUpdates();
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
  {
    // Make sure it's the root object that was updated.
    ui::AXTreeUpdate update = GetLastAccUpdate();
    EXPECT_EQ(root_obj.AxID(), update.nodes[0].id);
  }

  // If we reload the page and send a event, we should send
  // all 5 nodes to the browser. Also double-check that we didn't
  // leak any of the old BrowserTreeNodes.
  LoadHTML(html);
  document = GetMainFrame()->GetDocument();
  root_obj = WebAXObject::FromWebDocument(document);
  ClearHandledUpdates();
  SendPendingAccessibilityEvents();
  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());

  // Even if the first event is sent on an element other than
  // the root, the whole tree should be updated because we know
  // the browser doesn't have the root element.
  // When the entire page is reloaded like this, all of the nodes are sent.
  LoadHTML(html);
  document = GetMainFrame()->GetDocument();
  root_obj = WebAXObject::FromWebDocument(document);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());
  ClearHandledUpdates();

  // Now fire a single event and ensure that only one update is sent.
  const WebAXObject& first_child = root_obj.ChildAt(0);
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(first_child.AxID(), ax::mojom::Event::kFocus));
  SendPendingAccessibilityEvents();
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RenderAccessibilityImplTest, HideAccessibilityObject) {
  // Test RenderAccessibilityImpl and make sure it sends the
  // proper event to the browser when an object in the tree
  // is hidden, but its children are not.
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <div role="group" id="A">
          <div role="group" id="B" lang="en-US">
            <div role="group" id="C" style="visibility: visible" lang="fr-CA">
            </div>
          </div>
        </div>
      </body>
      )HTML");

  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());

  WebDocument document = GetMainFrame()->GetDocument();
  // Getting the root object will also force layout.
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html = root_obj.ChildAt(0);
  WebAXObject body = html.ChildAt(0);
  WebAXObject node_a = body.ChildAt(0);
  WebAXObject node_b = node_a.ChildAt(0);
  WebAXObject node_c = node_b.ChildAt(0);

  // Send a childrenChanged on "A".
  ClearHandledUpdates();
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(node_a);

  // Hide node "B" ("C" stays visible).
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'hidden';");

  SendPendingAccessibilityEvents();
  ui::AXTreeUpdate update = GetLastAccUpdate();
  ASSERT_EQ(2U, update.nodes.size());

  // Since ignored nodes are included in the ax tree with State::kIgnored set,
  // "C" is NOT reparented, only the changed nodes are re-serialized.
  // "A" updates because it handled dirty object
  // "B" updates because its State::kIgnored has changed
  EXPECT_EQ(0, update.node_id_to_clear);
  EXPECT_EQ(node_a.AxID(), update.nodes[0].id);
  EXPECT_EQ(node_b.AxID(), update.nodes[1].id);
  EXPECT_EQ(2, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RenderAccessibilityImplTest, ShowAccessibilityObject) {
  // Test RenderAccessibilityImpl and make sure it sends the
  // proper event to the browser when an object in the tree
  // is shown, causing its own already-visible children to be
  // reparented to it.
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <div role="group" id="A" aria-describedby="B">
          <div role="group" id="B" style="visibility: hidden" lang="en-US">
            <div role="group" id="C" style="visibility: visible" lang="fr-CA">
            </div>
          </div>
        </div>
      </body>
      )HTML");

  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());

  WebDocument document = GetMainFrame()->GetDocument();
  // Getting the root object also forces a layout.
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html = root_obj.ChildAt(0);
  WebAXObject body = html.ChildAt(0);
  WebAXObject node_a = body.ChildAt(0);
  WebAXObject node_b = node_a.ChildAt(0);
  WebAXObject node_c = node_b.ChildAt(0);

  // Send a childrenChanged on "A" and show node "B",
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(node_a);
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'visible';");

  ClearHandledUpdates();

  SendPendingAccessibilityEvents();
  ui::AXTreeUpdate update = GetLastAccUpdate();

  // Since ignored nodes are included in the ax tree with State::kIgnored set,
  // "C" is NOT reparented, only the changed nodes are re-serialized.
  // "A" updates because it handled the dirty
  // "B" updates because its State::kIgnored has changed
  ASSERT_EQ(2U, update.nodes.size());
  EXPECT_EQ(0, update.node_id_to_clear);
  EXPECT_EQ(node_a.AxID(), update.nodes[0].id);
  EXPECT_EQ(node_b.AxID(), update.nodes[1].id);
  EXPECT_EQ(2, CountAccessibilityNodesSentToBrowser());
}

// Tests if the bounds of the fixed positioned node is updated after scrolling.
TEST_F(RenderAccessibilityImplTest, TestBoundsForFixedNodeAfterScroll) {
  constexpr char html[] = R"HTML(
      <div id="positioned" style="position:fixed; top:10px; font-size:40px;"
        role="group" aria-label="first">title</div>
      <div style="padding-top: 50px; font-size:40px;">
        <h2>Heading #1</h2>
        <h2>Heading #2</h2>
        <h2>Heading #3</h2>
        <h2>Heading #4</h2>
        <h2>Heading #5</h2>
        <h2>Heading #6</h2>
        <h2>Heading #7</h2>
        <h2>Heading #8</h2>
      </div>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  int scroll_offset_y = 50;

  ui::AXNodeID expected_id = ui::kInvalidAXNodeID;
  ui::AXRelativeBounds expected_bounds;

  // Prepare the expected information from the tree.
  const std::vector<ui::AXTreeUpdate>& updates = GetHandledAccUpdates();
  for (const auto& update : base::Reversed(updates)) {
    for (const ui::AXNodeData& node : update.nodes) {
      if (node.GetStringAttribute(ax::mojom::StringAttribute::kName) ==
          "first") {
        expected_id = node.id;
        expected_bounds = node.relative_bounds;
        expected_bounds.bounds.set_y(expected_bounds.bounds.y() +
                                     scroll_offset_y);
        break;
      }
    }

    if (expected_id != ui::kInvalidAXNodeID)
      break;
  }

  ClearHandledUpdates();

  // Simulate scrolling down using JS.
  std::string js("window.scrollTo(0, " + base::NumberToString(scroll_offset_y) +
                 ");");
  ExecuteJavaScriptForTests(js.c_str());
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kScrollPositionChanged));
  SendPendingAccessibilityEvents();

  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());

  // Make sure it's the root object that was updated for scrolling.
  ui::AXTreeUpdate update = GetLastAccUpdate();
  EXPECT_EQ(root_obj.AxID(), update.nodes[0].id);

  // Make sure that a location change is sent for the fixed-positioned node.
  std::vector<ui::AXLocationChange>& changes = GetLocationChanges();
  EXPECT_EQ(changes.size(), 1u);
  EXPECT_EQ(changes[0].id, expected_id);
  EXPECT_EQ(changes[0].new_location, expected_bounds);
}

// Tests if the bounds are updated when it has multiple fixed nodes.
TEST_F(RenderAccessibilityImplTest, TestBoundsForMultipleFixedNodeAfterScroll) {
  constexpr char html[] = R"HTML(
    <div id="positioned" style="position:fixed; top:10px; font-size:40px;"
      role="group" aria-label="first">title1</div>
    <div id="positioned" style="position:fixed; top:50px; font-size:40px;"
      role="group" aria-label="second">title2</div>
    <div style="padding-top: 50px; font-size:40px;">
      <h2>Heading #1</h2>
      <h2>Heading #2</h2>
      <h2>Heading #3</h2>
      <h2>Heading #4</h2>
      <h2>Heading #5</h2>
      <h2>Heading #6</h2>
      <h2>Heading #7</h2>
      <h2>Heading #8</h2>
    </div>)HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  int scroll_offset_y = 50;

  std::map<ui::AXNodeID, ui::AXRelativeBounds> expected;

  // Prepare the expected information from the tree.
  const std::vector<ui::AXTreeUpdate>& updates = GetHandledAccUpdates();
  for (const ui::AXTreeUpdate& update : updates) {
    for (const ui::AXNodeData& node : update.nodes) {
      const std::string& name =
          node.GetStringAttribute(ax::mojom::StringAttribute::kName);
      if (name == "first" || name == "second") {
        ui::AXRelativeBounds ax_bounds = node.relative_bounds;
        ax_bounds.bounds.set_y(ax_bounds.bounds.y() + scroll_offset_y);
        expected[node.id] = ax_bounds;
      }
    }
  }

  ClearHandledUpdates();

  // Simulate scrolling down using JS.
  std::string js("window.scrollTo(0, " + base::NumberToString(scroll_offset_y) +
                 ");");
  ExecuteJavaScriptForTests(js.c_str());

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kScrollPositionChanged));
  SendPendingAccessibilityEvents();

  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());

  // Make sure it's the root object that was updated for scrolling.
  ui::AXTreeUpdate update = GetLastAccUpdate();
  EXPECT_EQ(root_obj.AxID(), update.nodes[0].id);

  // Make sure that a location change is sent for the fixed-positioned node.
  std::vector<ui::AXLocationChange>& changes = GetLocationChanges();
  EXPECT_EQ(changes.size(), 2u);
  for (auto& change : changes) {
    auto search = expected.find(change.id);
    EXPECT_NE(search, expected.end());
    EXPECT_EQ(search->second, change.new_location);
  }
}

TEST_F(RenderAccessibilityImplTest, TestFocusConsistency) {
  // Using aria-describedby ensures rhe ignored button is included in the tree.
  constexpr char html[] = R"HTML(
      <body>
        <a id="link" tabindex=0>link</a>
        <button id="button" style="visibility:hidden" tabindex=0
                aria-describedby="button">button</button>
        <script>
          link.addEventListener("click", () => {
            button.style.visibility = "visible";
            button.focus();
          });
        </script>
      </body>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  WebDocument document = GetMainFrame()->GetDocument();
  // Getting the root object also forces a layout.
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html_elem = root_obj.ChildAt(0);
  WebAXObject body = html_elem.ChildAt(0);
  WebAXObject link = body.ChildAt(0);
  WebAXObject button = body.ChildAt(1);

  // Set focus to the <a>, this will queue up an initial set of deferred
  // accessibility events to be queued up on AXObjectCacheImpl.
  ui::AXActionData action;
  action.target_node_id = link.AxID();
  action.action = ax::mojom::Action::kFocus;
  GetRenderAccessibilityImpl()->PerformAction(action);

  // Now perform the default action on the link, which will bounce focus to
  // the button element.
  action.target_node_id = link.AxID();
  action.action = ax::mojom::Action::kDoDefault;
  GetRenderAccessibilityImpl()->PerformAction(action);

  // The events and updates from the previous operation would normally be
  // processed in the next frame, but the initial focus operation caused a
  // ScheduleSendPendingAccessibilityEvents.
  SendPendingAccessibilityEvents();

  // The pattern up DOM/style updates above result in multiple AXTreeUpdates
  // sent over mojo. Search the updates to ensure that the button
  const std::vector<ui::AXTreeUpdate>& updates = GetHandledAccUpdates();
  ui::AXNodeID focused_node = ui::kInvalidAXNodeID;
  bool found_button_update = false;
  for (const auto& update : updates) {
    if (update.has_tree_data)
      focused_node = update.tree_data.focus_id;

    for (const auto& node_data : update.nodes) {
      if (node_data.id == button.AxID() &&
          !node_data.HasState(ax::mojom::State::kIgnored))
        found_button_update = true;
    }
  }

  EXPECT_EQ(focused_node, button.AxID());
  EXPECT_TRUE(found_button_update);
}

// Web popups don't exist on Android, so this test doesn't have to be run on
// this platform.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(RenderAccessibilityImplTest, TestHitTestPopupDoesNotCrash) {
  constexpr char html[] = R"HTML(
      <body>
      <select>
        <option>1</option>
        <option>2</option>
        <option>3</option>
        <option id="option_test">4</option>
      </select>
      <script>
        // Trigger endless layout updates in the popup so the cache is
        // processing deferred events.
        var option_test = document.getElementById("option_test");
        function update() {
          option_test.innerHTML = Math.random();
        }
        window.setInterval(update, 100);
      </script>
      </body>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  WebDocument document = GetMainFrame()->GetDocument();
  // Getting the root object also forces a layout.
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html_elem = root_obj.ChildAt(0);
  WebAXObject body = html_elem.ChildAt(0);
  WebAXObject select = body.ChildAt(0);

  // Open the popup.
  ui::AXActionData action;
  action.target_node_id = select.AxID();
  action.action = ax::mojom::Action::kDoDefault;
  GetRenderAccessibilityImpl()->PerformAction(action);

  blink::WebPagePopup* popup = frame()->GetWebView()->GetPagePopup();
  DCHECK_NE(popup, nullptr);

  // Hit test the popup and ignore the result. This test is ensuring that hit
  // testing can occur while processing deferred events, which means the cache
  // needs to be frozen.
  GetRenderAccessibilityImpl()->HitTest(
      select.GetBoundsInFrameCoordinates().CenterPoint(),
      ax::mojom::Event::kHover, /*request_id*/ 0,
      base::BindOnce(
          [](mojo::StructPtr<blink::mojom::HitTestResponse>) { return; }));
  SendPendingAccessibilityEvents();
}
#endif  // #if !BUILDFLAG(IS_ANDROID)

TEST_F(RenderAccessibilityImplTest, TestExpandCollapseTreeItem) {
  constexpr char html[] = R"HTML(
      <body>
        <div>
          <ol role="tree">
            <li role="treeitem" aria-expanded="false" id="1">
            </li>
          </ol>
        </div>
      </body>
    )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html_elem = root_obj.ChildAt(0);
  WebAXObject body = html_elem.ChildAt(0);
  WebAXObject div = body.ChildAt(0);
  WebAXObject ol = div.ChildAt(0);
  WebAXObject tree_item = ol.ChildAt(0);

  std::string js(
      "document.getElementById('1').addEventListener('keydown', (event) => { "
      "let item = "
      "document.getElementById('1'); if (event.key === 'ArrowRight') { "
      "item.setAttribute('aria-expanded','true');} else if (event.key === "
      "'ArrowLeft') { item.setAttribute('aria-expanded','false'); }}, true);");
  ExecuteJavaScriptForTests(js.c_str());

  // Expanding.
  ui::AXActionData action;
  action.target_node_id = tree_item.AxID();
  action.action = ax::mojom::Action::kExpand;
  GetRenderAccessibilityImpl()->PerformAction(action);
  SendPendingAccessibilityEvents();

  const std::vector<ui::AXTreeUpdate>& updates = GetHandledAccUpdates();
  bool found_expanded_update = false;
  for (const auto& update : updates) {
    for (const auto& node_data : update.nodes) {
      if (node_data.id == tree_item.AxID() &&
          node_data.HasState(ax::mojom::State::kExpanded)) {
        found_expanded_update = true;
      }
    }
  }

  EXPECT_TRUE(found_expanded_update);

  // Expanding when expanded
  action.target_node_id = tree_item.AxID();
  action.action = ax::mojom::Action::kExpand;
  GetRenderAccessibilityImpl()->PerformAction(action);
  SendPendingAccessibilityEvents();

  const std::vector<ui::AXTreeUpdate>& updates_2 = GetHandledAccUpdates();
  found_expanded_update = false;
  for (const auto& update : updates_2) {
    for (const auto& node_data : update.nodes) {
      if (node_data.id == tree_item.AxID() &&
          node_data.HasState(ax::mojom::State::kExpanded)) {
        found_expanded_update = true;
      }
    }
  }

  // Since item was already expanded, it should remain as such.
  EXPECT_TRUE(found_expanded_update);

  // Collapse when expanded.
  action.target_node_id = tree_item.AxID();
  action.action = ax::mojom::Action::kCollapse;
  GetRenderAccessibilityImpl()->PerformAction(action);
  SendPendingAccessibilityEvents();

  const std::vector<ui::AXTreeUpdate>& updates_3 = GetHandledAccUpdates();
  bool found_collapsed_update = false;
  for (const auto& update : updates_3) {
    for (const auto& node_data : update.nodes) {
      if (node_data.id == tree_item.AxID() &&
          node_data.HasState(ax::mojom::State::kCollapsed)) {
        found_collapsed_update = true;
      }
    }
  }

  // Element should have collapsed.
  EXPECT_TRUE(found_collapsed_update);

  // Collapse when collapsed.
  action.target_node_id = tree_item.AxID();
  action.action = ax::mojom::Action::kCollapse;
  GetRenderAccessibilityImpl()->PerformAction(action);
  SendPendingAccessibilityEvents();

  const std::vector<ui::AXTreeUpdate>& updates_4 = GetHandledAccUpdates();
  found_collapsed_update = false;
  for (const auto& update : updates_4) {
    for (const auto& node_data : update.nodes) {
      if (node_data.id == tree_item.AxID() &&
          node_data.HasState(ax::mojom::State::kCollapsed)) {
        found_collapsed_update = true;
      }
    }
  }

  // Element should still be collapsed.
  EXPECT_TRUE(found_collapsed_update);
}

class MockPluginAccessibilityTreeSource
    : public ui::
          AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>,
      public content::PluginAXTreeActionTargetAdapter {
 public:
  MockPluginAccessibilityTreeSource(ui::AXNodeID root_node_id) {
    ax_tree_ = std::make_unique<ui::AXTree>();
    root_node_ =
        std::make_unique<ui::AXNode>(ax_tree_.get(), nullptr, root_node_id, 0);
  }

  MockPluginAccessibilityTreeSource(const MockPluginAccessibilityTreeSource&) =
      delete;
  MockPluginAccessibilityTreeSource& operator=(
      const MockPluginAccessibilityTreeSource&) = delete;

  ~MockPluginAccessibilityTreeSource() override {}
  bool GetTreeData(ui::AXTreeData* data) const override { return true; }
  ui::AXNode* GetRoot() const override { return root_node_.get(); }
  ui::AXNode* GetFromId(ui::AXNodeID id) const override {
    return (root_node_->data().id == id) ? root_node_.get() : nullptr;
  }
  int32_t GetId(const ui::AXNode* node) const override {
    return root_node_->data().id;
  }
  void CacheChildrenIfNeeded(const ui::AXNode*) override {}
  size_t GetChildCount(const ui::AXNode* node) const override {
    return node->children().size();
  }
  const ui::AXNode* ChildAt(const ui::AXNode* node,
                            size_t index) const override {
    return node->children()[index];
  }
  void ClearChildCache(const ui::AXNode*) override {}
  ui::AXNode* GetParent(const ui::AXNode* node) const override {
    return nullptr;
  }
  bool IsEqual(const ui::AXNode* node1,
               const ui::AXNode* node2) const override {
    return (node1 == node2);
  }
  const ui::AXNode* GetNull() const override { return nullptr; }
  void SerializeNode(const ui::AXNode* node,
                     ui::AXNodeData* out_data) const override {
    DCHECK(node);
    *out_data = node->data();
  }
  void HandleAction(const ui::AXActionData& action_data) {}
  void ResetAccActionStatus() {}
  bool IsIgnored(const ui::AXNode* node) const override { return false; }
  std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      ui::AXNodeID id) override {
    action_target_called_ = true;
    return std::make_unique<ui::NullAXActionTarget>();
  }
  bool GetActionTargetCalled() { return action_target_called_; }
  void ResetActionTargetCalled() { action_target_called_ = false; }

 private:
  std::unique_ptr<ui::AXTree> ax_tree_;
  std::unique_ptr<ui::AXNode> root_node_;
  bool action_target_called_ = false;
};

TEST_F(RenderAccessibilityImplTest, TestAXActionTargetFromNodeId) {
  // Validate that we create the correct type of AXActionTarget for a given
  // node id.
  constexpr char html[] = R"HTML(
      <body>
      </body>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject body = root_obj.ChildAt(0);

  // An AxID for an HTML node should produce a Blink action target.
  std::unique_ptr<ui::AXActionTarget> body_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    body.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink, body_action_target->GetType());

  // An AxID for a Plugin node should produce a Plugin action target.
  ui::AXNodeID root_node_id = 100;
  MockPluginAccessibilityTreeSource pdf_acc_tree(root_node_id);
  //  GetRenderAccessibilityImpl()->SetPluginTreeSource(&pdf_acc_tree);

  // An AxId from Pdf, should call PdfAccessibilityTree::CreateActionTarget.
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, &pdf_acc_tree,
                                                    root_node_id);
  EXPECT_TRUE(pdf_acc_tree.GetActionTargetCalled());
  pdf_acc_tree.ResetActionTargetCalled();

  // An invalid AxID should produce a null action target.
  std::unique_ptr<ui::AXActionTarget> null_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, &pdf_acc_tree,
                                                    -1);
  EXPECT_EQ(ui::AXActionTarget::Type::kNull, null_action_target->GetType());
}

TEST_F(RenderAccessibilityImplTest, SendPendingAccessibilityEventsPostLoad) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <input id="text" value="Hello, World">
      </body>
      )HTML");

  // No logs initially.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents.PostLoad2", 0);

  // A load started event pauses logging.
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->DidCommitProvisionalLoad(
      ui::PAGE_TRANSITION_LINK);
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kLoadStart));
  SendPendingAccessibilityEvents();
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 1);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents.PostLoad2", 0);

  // Do not log in the serialization immediately following load completion.
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kLoadComplete));
  SendPendingAccessibilityEvents();
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 2);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents.PostLoad2", 0);

  // Now we start logging.
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 3);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents.PostLoad2", 1);

  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 4);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents.PostLoad2", 2);
}

class BlinkAXActionTargetTest : public RenderAccessibilityImplTest {
 protected:
  void SetUp() override {
    // Disable overlay scrollbars to avoid DCHECK on ChromeOS.
    feature_list_.InitAndDisableFeature(features::kOverlayScrollbar);

    RenderAccessibilityImplTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BlinkAXActionTargetTest, TestSetRangeValue) {
  constexpr char html[] = R"HTML(
      <body>
        <input type=range min=1 value=2 max=3 step=1>
      </body>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html_elem = root_obj.ChildAt(0);
  WebAXObject body = html_elem.ChildAt(0);
  WebAXObject input_range = body.ChildAt(0);

  float value = 0.0f;
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(2.0f, value);
  std::unique_ptr<ui::AXActionTarget> input_range_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    input_range.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_range_action_target->GetType());

  std::string value_to_set("1.0");
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kSetValue;
    action_data.value = value_to_set;
    EXPECT_TRUE(input_range.PerformAction(action_data));
  }
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(1.0f, value);

  SendPendingAccessibilityEvents();
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
  {
    // Make sure it's the input range object that was updated.
    ui::AXTreeUpdate update = GetLastAccUpdate();
    EXPECT_EQ(input_range.AxID(), update.nodes[0].id);
  }
}

TEST_F(BlinkAXActionTargetTest, TestMethods) {
  // Exercise the methods on BlinkAXActionTarget to ensure they have the
  // expected effects.
  constexpr char html[] = R"HTML(
      <body>
        <input type=checkbox>
        <input type=range min=1 value=2 max=3 step=1>
        <input type=text>
        <select size=2>
          <option>One</option>
          <option>Two</option>
        </select>
        <div style='width:100px; height: 100px; overflow:scroll'>
          <div style='width:1000px; height:900px'></div>
          <div style='width:1000px; height:100px'></div>
        </div>
        <div>Text Node One</div>
        <div>Text Node Two</div>
      </body>
      )HTML";
  LoadHTMLAndRefreshAccessibilityTree(html);

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject html_elem = root_obj.ChildAt(0);
  WebAXObject body = html_elem.ChildAt(0);
  WebAXObject input_checkbox = body.ChildAt(0);
  WebAXObject input_range = body.ChildAt(1);
  WebAXObject input_text = body.ChildAt(2);
  WebAXObject option = body.ChildAt(3).ChildAt(0).ChildAt(0);
  WebAXObject scroller = body.ChildAt(4);
  WebAXObject scroller_child = body.ChildAt(4).ChildAt(1);
  WebAXObject text_one = body.ChildAt(5).ChildAt(0);
  WebAXObject text_two = body.ChildAt(6).ChildAt(0);

  std::unique_ptr<ui::AXActionTarget> input_checkbox_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    input_checkbox.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_checkbox_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> input_range_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    input_range.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_range_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> input_text_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    input_text.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_text_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> option_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    option.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink, option_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> scroller_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    scroller.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            scroller_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> scroller_child_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    scroller_child.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            scroller_child_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> text_one_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    text_one.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            text_one_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> text_two_action_target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(document, nullptr,
                                                    text_two.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            text_two_action_target->GetType());

  EXPECT_EQ(ax::mojom::CheckedState::kFalse, input_checkbox.CheckedState());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDoDefault;
    EXPECT_TRUE(input_checkbox_action_target->PerformAction(action_data));
  }
  EXPECT_EQ(ax::mojom::CheckedState::kTrue, input_checkbox.CheckedState());

  float value = 0.0f;
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(2.0f, value);
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDecrement;
    EXPECT_TRUE(input_range_action_target->PerformAction(action_data));
  }
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(1.0f, value);
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kIncrement;
    EXPECT_TRUE(input_range_action_target->PerformAction(action_data));
  }
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(2.0f, value);

  EXPECT_FALSE(input_range.IsFocused());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kFocus;
    EXPECT_TRUE(input_range_action_target->PerformAction(action_data));
  }
  EXPECT_TRUE(input_range.IsFocused());

  {
    // Blurring an element requires layout to be clean.
    GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kBlur;
    EXPECT_TRUE(input_range_action_target->PerformAction(action_data));
  }
  EXPECT_FALSE(input_range.IsFocused());

  // Increment/decrement actions produce synthesized keydown and keyup events,
  // and the keyup event is delayed 100ms to look more natural. We need to wait
  // for them to happen to finish the test cleanly in the TearDown phase.
  task_environment_.FastForwardBy(base::Seconds(1));
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();

  gfx::RectF expected_bounds;
  blink::WebAXObject offset_container;
  gfx::Transform container_transform;
  input_checkbox.GetRelativeBounds(offset_container, expected_bounds,
                                   container_transform);
  gfx::Rect actual_bounds = input_checkbox_action_target->GetRelativeBounds();
  EXPECT_EQ(static_cast<int>(expected_bounds.x()), actual_bounds.x());
  EXPECT_EQ(static_cast<int>(expected_bounds.y()), actual_bounds.y());
  EXPECT_EQ(static_cast<int>(expected_bounds.width()), actual_bounds.width());
  EXPECT_EQ(static_cast<int>(expected_bounds.height()), actual_bounds.height());

  gfx::Point offset_to_set(500, 500);
  scroller_action_target->SetScrollOffset(gfx::Point(500, 500));
  EXPECT_EQ(offset_to_set, scroller_action_target->GetScrollOffset());
  EXPECT_EQ(gfx::Point(0, 0), scroller_action_target->MinimumScrollOffset());
  EXPECT_GE(scroller_action_target->MaximumScrollOffset().y(), 900);

  std::string value_to_set("test-value");
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kSetValue;
    action_data.value = value_to_set;
    EXPECT_TRUE(input_text_action_target->PerformAction(action_data));
  }
  EXPECT_EQ(value_to_set, input_text.GetValueForControl().Utf8());

  // Setting selection requires layout to be clean.
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();

  EXPECT_TRUE(text_one_action_target->SetSelection(
      text_one_action_target.get(), 3, text_two_action_target.get(), 4));
  bool is_selection_backward;
  blink::WebAXObject anchor_object;
  int anchor_offset;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset;
  ax::mojom::TextAffinity focus_affinity;
  root_obj.Selection(is_selection_backward, anchor_object, anchor_offset,
                     anchor_affinity, focus_object, focus_offset,
                     focus_affinity);
  EXPECT_EQ(text_one, anchor_object);
  EXPECT_EQ(3, anchor_offset);
  EXPECT_EQ(text_two, focus_object);
  EXPECT_EQ(4, focus_offset);

  scroller_action_target->SetScrollOffset(gfx::Point(0, 0));
  EXPECT_EQ(gfx::Point(0, 0), scroller_action_target->GetScrollOffset());
  EXPECT_TRUE(scroller_child_action_target->ScrollToMakeVisible());
  EXPECT_GE(scroller_action_target->GetScrollOffset().y(), 900);

  scroller_action_target->SetScrollOffset(gfx::Point(0, 0));
  EXPECT_EQ(gfx::Point(0, 0), scroller_action_target->GetScrollOffset());
  EXPECT_TRUE(scroller_child_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50), ax::mojom::ScrollAlignment::kScrollAlignmentLeft,
      ax::mojom::ScrollAlignment::kScrollAlignmentTop,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  EXPECT_GE(scroller_action_target->GetScrollOffset().y(), 900);

  scroller_action_target->SetScrollOffset(gfx::Point(0, 0));
  EXPECT_EQ(gfx::Point(0, 0), scroller_action_target->GetScrollOffset());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kScrollToPoint;
    action_data.target_point = gfx::Point(0, 0);
    EXPECT_TRUE(scroller_child_action_target->PerformAction(action_data));
  }
  EXPECT_GE(scroller_action_target->GetScrollOffset().y(), 900);
}

// URL-keyed metrics recorder implementation that just counts the number
// of times it's been called.
class MockUkmRecorder : public ukm::MojoUkmRecorder {
 public:
  MockUkmRecorder(ukm::mojom::UkmRecorderFactory& factory)
      : MojoUkmRecorder(factory) {}
  void AddEntry(ukm::mojom::UkmEntryPtr entry) override { calls_++; }

  int calls() const { return calls_; }

 private:
  int calls_ = 0;
};

// Tests for URL-keyed metrics.
class RenderAccessibilityImplUKMTest : public RenderAccessibilityImplTest {
 public:
  void SetUp() override {
    RenderAccessibilityImplTest::SetUp();
    mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
    std::ignore = factory.BindNewPipeAndPassReceiver();
    GetRenderAccessibilityImpl()->ukm_recorder_ =
        std::make_unique<MockUkmRecorder>(*factory);
  }

  void TearDown() override { RenderAccessibilityImplTest::TearDown(); }

  MockUkmRecorder* ukm_recorder() {
    return static_cast<MockUkmRecorder*>(
        GetRenderAccessibilityImpl()->ukm_recorder_.get());
  }

  void SetTimeDelayForNextSerialize(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
    GetRenderAccessibilityImpl()->slowest_serialization_time_ = delta;
  }
};

TEST_F(RenderAccessibilityImplUKMTest, TestFireUKMs) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <input id="text" value="Hello, World">
      </body>
      )HTML");

  // No URL-keyed metrics should be fired initially.
  EXPECT_EQ(0, ukm_recorder()->calls());
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 0);

  // No URL-keyed metrics should be fired after we send one event.
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(0, ukm_recorder()->calls());
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 1);

  // No URL-keyed metrics should be fired even after an event that takes
  // 300 ms, but we should now have something to send.
  // This must be >= kMinSerializationTimeToSendInMS
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  SetTimeDelayForNextSerialize(base::Milliseconds(300));
  EXPECT_EQ(0, ukm_recorder()->calls());
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 2);

  // After 1000 seconds have passed, the next time we send an event we should
  // send URL-keyed metrics.
  task_environment_.FastForwardBy(base::Seconds(1000));
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(1, ukm_recorder()->calls());
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 3);

  // Send another event that takes a long (simulated) time to serialize.
  // This must be >= kMinSerializationTimeToSend
  SetTimeDelayForNextSerialize(base::Milliseconds(200));
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj);
  SendPendingAccessibilityEvents();
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents2", 4);

  // We shouldn't have a new call to the UKM recorder yet, not enough
  // time has elapsed.
  EXPECT_EQ(1, ukm_recorder()->calls());

  // Navigate to a new page.
  GetRenderAccessibilityImpl()->DidCommitProvisionalLoad(
      ui::PAGE_TRANSITION_LINK);

  // Now we should have yet another UKM recorded because of the page
  // transition.
  EXPECT_EQ(2, ukm_recorder()->calls());
}

}  // namespace content
