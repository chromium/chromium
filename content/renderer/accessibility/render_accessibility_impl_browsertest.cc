// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/test/fake_pepper_plugin_instance.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/null_ax_action_target.h"
#include "ui/native_theme/native_theme_features.h"

namespace content {

using blink::WebAXObject;
using blink::WebDocument;
using testing::ElementsAre;

class TestRenderAccessibilityImpl : public RenderAccessibilityImpl {
 public:
  explicit TestRenderAccessibilityImpl(RenderFrameImpl* render_frame)
      : RenderAccessibilityImpl(render_frame, ui::kAXModeComplete) {}
  ~TestRenderAccessibilityImpl() override = default;

  // Change method's visibility from protected to public so that it can be
  // accessed by tests.
  void SendPendingAccessibilityEvents() {
    RenderAccessibilityImpl::SendPendingAccessibilityEvents();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRenderAccessibilityImpl);
};

class TestAXImageAnnotator : public AXImageAnnotator {
 public:
  TestAXImageAnnotator(
      TestRenderAccessibilityImpl* const render_accessibility,
      mojo::PendingRemote<image_annotation::mojom::Annotator> annotator)
      : AXImageAnnotator(render_accessibility,
                         std::string() /* preferred_language */,
                         std::move(annotator)) {}
  ~TestAXImageAnnotator() override = default;

 private:
  std::string GenerateImageSourceId(
      const blink::WebAXObject& image) const override {
    std::string image_id;
    if (image.IsDetached() || image.IsNull() || image.GetNode().IsNull() ||
        image.GetNode().To<blink::WebElement>().IsNull()) {
      ADD_FAILURE() << "Unable to retrieve the image src.";
      return image_id;
    }

    image_id =
        image.GetNode().To<blink::WebElement>().GetAttribute("SRC").Utf8();
    return image_id;
  }

  DISALLOW_COPY_AND_ASSIGN(TestAXImageAnnotator);
};

class MockAnnotationService : public image_annotation::mojom::Annotator {
 public:
  MockAnnotationService() = default;
  ~MockAnnotationService() override = default;

  mojo::PendingRemote<image_annotation::mojom::Annotator> GetRemote() {
    mojo::PendingRemote<image_annotation::mojom::Annotator> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void AnnotateImage(
      const std::string& image_id,
      const std::string& /* description_language_tag */,
      mojo::PendingRemote<image_annotation::mojom::ImageProcessor>
          image_processor,
      AnnotateImageCallback callback) override {
    image_ids_.push_back(image_id);
    image_processors_.push_back(
        mojo::Remote<image_annotation::mojom::ImageProcessor>(
            std::move(image_processor)));
    image_processors_.back().set_disconnect_handler(
        base::BindOnce(&MockAnnotationService::ResetImageProcessor,
                       base::Unretained(this), image_processors_.size() - 1));
    callbacks_.push_back(std::move(callback));
  }

  // Tests should not delete entries in these lists.
  std::vector<std::string> image_ids_;
  std::vector<mojo::Remote<image_annotation::mojom::ImageProcessor>>
      image_processors_;
  std::vector<AnnotateImageCallback> callbacks_;

 private:
  void ResetImageProcessor(const size_t index) {
    image_processors_[index].reset();
  }

  mojo::ReceiverSet<image_annotation::mojom::Annotator> receivers_;

  DISALLOW_COPY_AND_ASSIGN(MockAnnotationService);
};

class RenderAccessibilityImplTest : public RenderViewTest {
 public:
  RenderAccessibilityImplTest() = default;
  ~RenderAccessibilityImplTest() override = default;

 protected:
  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(view()->GetMainRenderFrame());
  }

  // Loads a page given an HTML snippet and initializes its accessibility tree.
  //
  // Consolidates the initialization code required by all tests into a single
  // method.
  void LoadHTMLAndRefreshAccessibilityTree(const char* html) {
    LoadHTML(html);
    sink_->ClearMessages();
    WebDocument document = GetMainFrame()->GetDocument();
    EXPECT_FALSE(document.IsNull());
    WebAXObject root_obj = WebAXObject::FromWebDocument(document);
    EXPECT_FALSE(root_obj.IsNull());
    render_accessibility().HandleAXEvent(root_obj,
                                         ax::mojom::Event::kLayoutComplete);
    render_accessibility().SendPendingAccessibilityEvents();
  }

  void SetUp() override {
    RenderViewTest::SetUp();
    sink_ = &render_thread_->sink();
    render_accessibility_ =
        std::make_unique<TestRenderAccessibilityImpl>(frame());
  }

  void TearDown() override {
    render_accessibility_.release();
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  void SetMode(ui::AXMode mode) { frame()->OnSetAccessibilityMode(mode); }

  void GetLastAccessibilityEventBundle(
      AccessibilityHostMsg_EventBundleParams* event_bundle) {
    const IPC::Message* message =
        sink_->GetUniqueMessageMatching(AccessibilityHostMsg_EventBundle::ID);
    ASSERT_TRUE(message);
    std::tuple<AccessibilityHostMsg_EventBundleParams, int, int> param;
    AccessibilityHostMsg_EventBundle::Read(message, &param);
    *event_bundle = std::get<0>(param);
  }

  AXContentTreeUpdate GetLastAccUpdate() {
    AccessibilityHostMsg_EventBundleParams event_bundle;
    GetLastAccessibilityEventBundle(&event_bundle);
    CHECK_GE(event_bundle.updates.size(), 1U);
    return event_bundle.updates[event_bundle.updates.size() - 1];
  }

  int CountAccessibilityNodesSentToBrowser() {
    AXContentTreeUpdate update = GetLastAccUpdate();
    return update.nodes.size();
  }

  TestRenderAccessibilityImpl& render_accessibility() {
    return *render_accessibility_;
  }

  IPC::TestSink* sink_;

 private:
  std::unique_ptr<TestRenderAccessibilityImpl> render_accessibility_;

  DISALLOW_COPY_AND_ASSIGN(RenderAccessibilityImplTest);
};

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

  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());

  // If we post another event but the tree doesn't change,
  // we should only send 1 node to the browser.
  sink_->ClearMessages();
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  render_accessibility().HandleAXEvent(root_obj,
                                       ax::mojom::Event::kLayoutComplete);
  render_accessibility().SendPendingAccessibilityEvents();
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
  {
    // Make sure it's the root object that was updated.
    AXContentTreeUpdate update = GetLastAccUpdate();
    EXPECT_EQ(root_obj.AxID(), update.nodes[0].id);
  }

  // If we reload the page and send a event, we should send
  // all 5 nodes to the browser. Also double-check that we didn't
  // leak any of the old BrowserTreeNodes.
  LoadHTML(html);
  document = GetMainFrame()->GetDocument();
  root_obj = WebAXObject::FromWebDocument(document);
  sink_->ClearMessages();
  render_accessibility().HandleAXEvent(root_obj,
                                       ax::mojom::Event::kLayoutComplete);
  render_accessibility().SendPendingAccessibilityEvents();
  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());

  // Even if the first event is sent on an element other than
  // the root, the whole tree should be updated because we know
  // the browser doesn't have the root element.
  LoadHTML(html);
  document = GetMainFrame()->GetDocument();
  root_obj = WebAXObject::FromWebDocument(document);
  sink_->ClearMessages();
  const WebAXObject& first_child = root_obj.ChildAt(0);
  render_accessibility().HandleAXEvent(first_child,
                                       ax::mojom::Event::kLiveRegionChanged);
  render_accessibility().SendPendingAccessibilityEvents();
  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RenderAccessibilityImplTest, HideAccessibilityObject) {
  // Test RenderAccessibilityImpl and make sure it sends the
  // proper event to the browser when an object in the tree
  // is hidden, but its children are not.
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <div role="group" id="A">
          <div role="group" id="B">
            <div role="group" id="C" style="visibility: visible">
            </div>
          </div>
        </div>
      </body>
      )HTML");

  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject body = root_obj.ChildAt(0);
  WebAXObject node_a = body.ChildAt(0);
  WebAXObject node_b = node_a.ChildAt(0);
  WebAXObject node_c = node_b.ChildAt(0);

  // Hide node "B" ("C" stays visible).
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'hidden';");
  // Force layout now.
  root_obj.UpdateLayoutAndCheckValidity();

  // Send a childrenChanged on "A".
  sink_->ClearMessages();
  render_accessibility().HandleAXEvent(node_a,
                                       ax::mojom::Event::kChildrenChanged);
  render_accessibility().SendPendingAccessibilityEvents();
  AXContentTreeUpdate update = GetLastAccUpdate();
  ASSERT_EQ(2U, update.nodes.size());

  // Since ignored nodes are included in the ax tree with State::kIgnored set,
  // "C" is NOT reparented, only the changed nodes are re-serialized.
  // "A" updates because it handled Event::kChildrenChanged
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
        <div role="group" id="A">
          <div role="group" id="B" style="visibility: hidden">
            <div role="group" id="C" style="visibility: visible">
            </div>
          </div>
        </div>
      </body>
      )HTML");

  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  WebAXObject body = root_obj.ChildAt(0);
  WebAXObject node_a = body.ChildAt(0);
  WebAXObject node_b = node_a.ChildAt(0);
  WebAXObject node_c = node_b.ChildAt(0);

  // Show node "B", then send a childrenChanged on "A".
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'visible';");

  root_obj.UpdateLayoutAndCheckValidity();
  sink_->ClearMessages();

  render_accessibility().HandleAXEvent(node_a,
                                       ax::mojom::Event::kChildrenChanged);
  render_accessibility().SendPendingAccessibilityEvents();
  AXContentTreeUpdate update = GetLastAccUpdate();

  // Since ignored nodes are included in the ax tree with State::kIgnored set,
  // "C" is NOT reparented, only the changed nodes are re-serialized.
  // "A" updates because it handled Event::kChildrenChanged
  // "B" updates because its State::kIgnored has changed
  ASSERT_EQ(2U, update.nodes.size());
  EXPECT_EQ(0, update.node_id_to_clear);
  EXPECT_EQ(node_a.AxID(), update.nodes[0].id);
  EXPECT_EQ(node_b.AxID(), update.nodes[1].id);
  EXPECT_EQ(2, CountAccessibilityNodesSentToBrowser());
}

class MockPluginAccessibilityTreeSource : public content::PluginAXTreeSource {
 public:
  MockPluginAccessibilityTreeSource(ui::AXNode::AXID root_node_id) {
    ax_tree_ = std::make_unique<ui::AXTree>();
    root_node_ =
        std::make_unique<ui::AXNode>(ax_tree_.get(), nullptr, root_node_id, 0);
  }
  ~MockPluginAccessibilityTreeSource() override {}
  bool GetTreeData(ui::AXTreeData* data) const override { return true; }
  ui::AXNode* GetRoot() const override { return root_node_.get(); }
  ui::AXNode* GetFromId(ui::AXNode::AXID id) const override {
    return (root_node_->data().id == id) ? root_node_.get() : nullptr;
  }
  int32_t GetId(const ui::AXNode* node) const override {
    return root_node_->data().id;
  }
  void GetChildren(
      const ui::AXNode* node,
      std::vector<const ui::AXNode*>* out_children) const override {
    DCHECK(node);
    *out_children = std::vector<const ui::AXNode*>(node->children().cbegin(),
                                                   node->children().cend());
  }
  ui::AXNode* GetParent(const ui::AXNode* node) const override {
    return nullptr;
  }
  bool IsValid(const ui::AXNode* node) const override { return true; }
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
      const ui::AXNode& target_node) override {
    action_target_called_ = true;
    return std::make_unique<ui::NullAXActionTarget>();
  }
  bool GetActionTargetCalled() { return action_target_called_; }
  void ResetActionTargetCalled() { action_target_called_ = false; }

 private:
  std::unique_ptr<ui::AXTree> ax_tree_;
  std::unique_ptr<ui::AXNode> root_node_;
  bool action_target_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(MockPluginAccessibilityTreeSource);
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
      AXActionTargetFactory::CreateFromNodeId(document, nullptr, body.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink, body_action_target->GetType());

  // An AxID for a Plugin node should produce a Plugin action target.
  ui::AXNode::AXID root_node_id = render_accessibility().GenerateAXID();
  MockPluginAccessibilityTreeSource pdf_acc_tree(root_node_id);
  render_accessibility().SetPluginTreeSource(&pdf_acc_tree);

  // An AxId from Pdf, should call PdfAccessibilityTree::CreateActionTarget.
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, &pdf_acc_tree,
                                              root_node_id);
  EXPECT_TRUE(pdf_acc_tree.GetActionTargetCalled());
  pdf_acc_tree.ResetActionTargetCalled();

  // An invalid AxID should produce a null action target.
  std::unique_ptr<ui::AXActionTarget> null_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, &pdf_acc_tree, -1);
  EXPECT_EQ(ui::AXActionTarget::Type::kNull, null_action_target->GetType());
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
  WebAXObject body = root_obj.ChildAt(0);
  WebAXObject input_checkbox = body.ChildAt(0);
  WebAXObject input_range = body.ChildAt(1);
  WebAXObject input_text = body.ChildAt(2);
  WebAXObject option = body.ChildAt(3).ChildAt(0);
  WebAXObject scroller = body.ChildAt(4);
  WebAXObject scroller_child = body.ChildAt(4).ChildAt(1);
  WebAXObject text_one = body.ChildAt(5).ChildAt(0);
  WebAXObject text_two = body.ChildAt(6).ChildAt(0);

  std::unique_ptr<ui::AXActionTarget> input_checkbox_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              input_checkbox.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_checkbox_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> input_range_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              input_range.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_range_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> input_text_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              input_text.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            input_text_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> option_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr, option.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink, option_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> scroller_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              scroller.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            scroller_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> scroller_child_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              scroller_child.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            scroller_child_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> text_one_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              text_one.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            text_one_action_target->GetType());

  std::unique_ptr<ui::AXActionTarget> text_two_action_target =
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
                                              text_two.AxID());
  EXPECT_EQ(ui::AXActionTarget::Type::kBlink,
            text_two_action_target->GetType());

  EXPECT_EQ(ax::mojom::CheckedState::kFalse, input_checkbox.CheckedState());
  EXPECT_TRUE(input_checkbox_action_target->Click());
  EXPECT_EQ(ax::mojom::CheckedState::kTrue, input_checkbox.CheckedState());

  float value = 0.0f;
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(2.0f, value);
  EXPECT_TRUE(input_range_action_target->Decrement());
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(1.0f, value);
  EXPECT_TRUE(input_range_action_target->Increment());
  EXPECT_TRUE(input_range.ValueForRange(&value));
  EXPECT_EQ(2.0f, value);

  EXPECT_FALSE(input_range.IsFocused());
  EXPECT_TRUE(input_range_action_target->Focus());
  EXPECT_TRUE(input_range.IsFocused());

  blink::WebFloatRect expected_bounds;
  blink::WebAXObject offset_container;
  SkMatrix44 container_transform;
  input_checkbox.GetRelativeBounds(offset_container, expected_bounds,
                                   container_transform);
  gfx::Rect actual_bounds = input_checkbox_action_target->GetRelativeBounds();
  EXPECT_EQ(static_cast<int>(expected_bounds.x), actual_bounds.x());
  EXPECT_EQ(static_cast<int>(expected_bounds.y), actual_bounds.y());
  EXPECT_EQ(static_cast<int>(expected_bounds.width), actual_bounds.width());
  EXPECT_EQ(static_cast<int>(expected_bounds.height), actual_bounds.height());

  gfx::Point offset_to_set(500, 500);
  scroller_action_target->SetScrollOffset(gfx::Point(500, 500));
  EXPECT_EQ(offset_to_set, scroller_action_target->GetScrollOffset());
  EXPECT_EQ(gfx::Point(0, 0), scroller_action_target->MinimumScrollOffset());
  EXPECT_GE(scroller_action_target->MaximumScrollOffset().y(), 900);

  // Android does not produce accessible items for option elements.
#if !defined(OS_ANDROID)
  EXPECT_EQ(blink::kWebAXSelectedStateFalse, option.IsSelected());
  EXPECT_TRUE(option_action_target->SetSelected(true));
  EXPECT_EQ(blink::kWebAXSelectedStateTrue, option.IsSelected());
#endif

  std::string value_to_set("test-value");
  input_text_action_target->SetValue(value_to_set);
  EXPECT_EQ(value_to_set, input_text.StringValue().Utf8());

  // Setting selection requires layout to be clean.
  ASSERT_TRUE(root_obj.UpdateLayoutAndCheckValidity());

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
      ax::mojom::ScrollAlignment::kScrollAlignmentTop));
  EXPECT_GE(scroller_action_target->GetScrollOffset().y(), 900);

  scroller_action_target->SetScrollOffset(gfx::Point(0, 0));
  EXPECT_EQ(gfx::Point(0, 0), scroller_action_target->GetScrollOffset());
  EXPECT_TRUE(
      scroller_child_action_target->ScrollToGlobalPoint(gfx::Point(0, 0)));
  EXPECT_GE(scroller_action_target->GetScrollOffset().y(), 900);
}

//
// AXImageAnnotatorTest
//

class AXImageAnnotatorTest : public RenderAccessibilityImplTest {
 public:
  AXImageAnnotatorTest() = default;
  ~AXImageAnnotatorTest() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalAccessibilityLabels);
    RenderAccessibilityImplTest::SetUp();
    // TODO(nektar): Add the ability to test the AX action that labels images
    // only once.
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    SetMode(mode);
    render_accessibility().ax_image_annotator_ =
        std::make_unique<TestAXImageAnnotator>(&render_accessibility(),
                                               mock_annotator().GetRemote());
    render_accessibility().tree_source_.RemoveImageAnnotator();
    render_accessibility().tree_source_.AddImageAnnotator(
        render_accessibility().ax_image_annotator_.get());
  }

  void TearDown() override {
    render_accessibility().ax_image_annotator_.release();
    RenderAccessibilityImplTest::TearDown();
  }

  MockAnnotationService& mock_annotator() { return mock_annotator_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockAnnotationService mock_annotator_;

  DISALLOW_COPY_AND_ASSIGN(AXImageAnnotatorTest);
};

TEST_F(AXImageAnnotatorTest, OnImageAdded) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <p>Test document</p>
        <img id="A" src="test1.jpg"
            style="width: 200px; height: 150px;">
        <img id="B" src="test2.jpg"
            style="visibility: hidden; width: 200px; height: 150px;">
      </body>
      )HTML");

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_, ElementsAre("test1.jpg"));
  ASSERT_EQ(1u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_EQ(1u, mock_annotator().callbacks_.size());

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  ASSERT_FALSE(root_obj.IsNull());

  // Show node "B".
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'visible';");
  sink_->ClearMessages();
  root_obj.UpdateLayoutAndCheckValidity();

  // This should update the annotations of all images on the page, including the
  // already visible one.
  render_accessibility().MarkWebAXObjectDirty(root_obj, true /* subtree */);
  render_accessibility().SendPendingAccessibilityEvents();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_,
              ElementsAre("test1.jpg", "test1.jpg", "test2.jpg"));
  ASSERT_EQ(3u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[2].is_bound());
  EXPECT_EQ(3u, mock_annotator().callbacks_.size());
}

TEST_F(AXImageAnnotatorTest, OnImageUpdated) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <p>Test document</p>
        <img id="A" src="test1.jpg"
            style="width: 200px; height: 150px;">
      </body>
      )HTML");

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_, ElementsAre("test1.jpg"));
  ASSERT_EQ(1u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_EQ(1u, mock_annotator().callbacks_.size());

  sink_->ClearMessages();
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  ASSERT_FALSE(root_obj.IsNull());
  // This should update the annotations of all images on the page.
  render_accessibility().MarkWebAXObjectDirty(root_obj, true /* subtree */);
  render_accessibility().SendPendingAccessibilityEvents();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_,
              ElementsAre("test1.jpg", "test1.jpg"));
  ASSERT_EQ(2u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[1].is_bound());
  EXPECT_EQ(2u, mock_annotator().callbacks_.size());

  // Update node "A".
  ExecuteJavaScriptForTests("document.querySelector('img').src = 'test2.jpg';");

  sink_->ClearMessages();
  // This should update the annotations of all images on the page, including the
  // now updated image src.
  render_accessibility().MarkWebAXObjectDirty(root_obj, true /* subtree */);
  render_accessibility().SendPendingAccessibilityEvents();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_,
              ElementsAre("test1.jpg", "test1.jpg", "test2.jpg"));
  ASSERT_EQ(3u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[2].is_bound());
  EXPECT_EQ(3u, mock_annotator().callbacks_.size());
}

}  // namespace content
