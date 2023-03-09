// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/test/test_render_frame.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-test-utils.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/null_ax_action_target.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace content {

using blink::WebAXObject;
using blink::WebDocument;
using testing::ElementsAre;

namespace {

#if !BUILDFLAG(IS_ANDROID)
bool IsSelected(const WebAXObject& obj) {
  ui::AXNodeData node_data;
  obj.Serialize(&node_data, ui::kAXModeComplete);
  return node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

class TestAXImageAnnotator : public AXImageAnnotator {
 public:
  TestAXImageAnnotator(
      RenderAccessibilityImpl* const render_accessibility,
      mojo::PendingRemote<image_annotation::mojom::Annotator> annotator)
      : AXImageAnnotator(render_accessibility,
                         std::move(annotator)) {}

  TestAXImageAnnotator(const TestAXImageAnnotator&) = delete;
  TestAXImageAnnotator& operator=(const TestAXImageAnnotator&) = delete;

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
};

class MockAnnotationService : public image_annotation::mojom::Annotator {
 public:
  MockAnnotationService() = default;

  MockAnnotationService(const MockAnnotationService&) = delete;
  MockAnnotationService& operator=(const MockAnnotationService&) = delete;

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
};

class RenderAccessibilityHostInterceptor
    : public blink::mojom::RenderAccessibilityHostInterceptorForTesting {
 public:
  explicit RenderAccessibilityHostInterceptor(
      blink::BrowserInterfaceBrokerProxy* broker) {
    broker->GetInterface(local_frame_host_remote_.BindNewPipeAndPassReceiver());
    broker->SetBinderForTesting(
        blink::mojom::RenderAccessibilityHost::Name_,
        base::BindRepeating(&RenderAccessibilityHostInterceptor::
                                BindRenderAccessibilityHostReceiver,
                            base::Unretained(this)));
  }
  ~RenderAccessibilityHostInterceptor() override = default;

  blink::mojom::RenderAccessibilityHost* GetForwardingInterface() override {
    return local_frame_host_remote_.get();
  }

  void BindRenderAccessibilityHostReceiver(
      mojo::ScopedMessagePipeHandle handle) {
    receiver_.Add(this,
                  mojo::PendingReceiver<blink::mojom::RenderAccessibilityHost>(
                      std::move(handle)));

    receiver_.set_disconnect_handler(base::BindRepeating(
        [](mojo::ReceiverSet<blink::mojom::RenderAccessibilityHost>* receiver) {
        },
        base::Unretained(&receiver_)));
  }

  void HandleAXEvents(blink::mojom::AXUpdatesAndEventsPtr updates_and_events,
                      int32_t reset_token,
                      HandleAXEventsCallback callback) override {
    handled_updates_.insert(handled_updates_.end(),
                            updates_and_events->updates.begin(),
                            updates_and_events->updates.end());
    std::move(callback).Run();
  }

  void HandleAXLocationChanges(
      std::vector<blink::mojom::LocationChangesPtr> changes) override {
    for (auto& change : changes)
      location_changes_.emplace_back(std::move(change));
  }

  ui::AXTreeUpdate& last_update() {
    CHECK_GE(handled_updates_.size(), 1U);
    return handled_updates_.back();
  }

  const std::vector<ui::AXTreeUpdate>& handled_updates() const {
    return handled_updates_;
  }

  std::vector<blink::mojom::LocationChangesPtr>& location_changes() {
    return location_changes_;
  }

  void ClearHandledUpdates() { handled_updates_.clear(); }

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::ReceiverSet<blink::mojom::RenderAccessibilityHost> receiver_;
  mojo::Remote<blink::mojom::RenderAccessibilityHost> local_frame_host_remote_;

  std::vector<::ui::AXTreeUpdate> handled_updates_;
  std::vector<blink::mojom::LocationChangesPtr> location_changes_;
};

class RenderAccessibilityTestRenderFrame : public TestRenderFrame {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params) {
    return new RenderAccessibilityTestRenderFrame(std::move(params));
  }

  ~RenderAccessibilityTestRenderFrame() override = default;

  ui::AXTreeUpdate& LastUpdate() {
    return render_accessibility_host_->last_update();
  }

  const std::vector<ui::AXTreeUpdate>& HandledUpdates() {
    return render_accessibility_host_->handled_updates();
  }

  void ClearHandledUpdates() {
    render_accessibility_host_->ClearHandledUpdates();
  }

  std::vector<blink::mojom::LocationChangesPtr>& LocationChanges() {
    return render_accessibility_host_->location_changes();
  }

 private:
  explicit RenderAccessibilityTestRenderFrame(
      RenderFrameImpl::CreateParams params)
      : TestRenderFrame(std::move(params)),
        render_accessibility_host_(
            std::make_unique<RenderAccessibilityHostInterceptor>(
                GetBrowserInterfaceBroker())) {}

  std::unique_ptr<RenderAccessibilityHostInterceptor>
      render_accessibility_host_;
};

class RenderAccessibilityImplTest : public RenderViewTest {
 public:
  RenderAccessibilityImplTest()
      : RenderViewTest(/*hook_render_frame_creation=*/false) {
    RenderFrameImpl::InstallCreateHook(
        &RenderAccessibilityTestRenderFrame::CreateTestRenderFrame);
  }

  RenderAccessibilityImplTest(const RenderAccessibilityImplTest&) = delete;
  RenderAccessibilityImplTest& operator=(const RenderAccessibilityImplTest&) =
      delete;

  ~RenderAccessibilityImplTest() override = default;

 protected:
  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(RenderViewTest::GetMainRenderFrame());
  }

  IPC::TestSink* sink() { return sink_; }

  RenderAccessibilityImpl* GetRenderAccessibilityImpl() {
    auto* accessibility_manager = frame()->GetRenderAccessibilityManager();
    DCHECK(accessibility_manager);
    return accessibility_manager->GetRenderAccessibilityImpl();
  }

  // Loads a page given an HTML snippet and initializes its accessibility tree.
  //
  // Consolidates the initialization code required by all tests into a single
  // method.
  void LoadHTMLAndRefreshAccessibilityTree(const char* html) {
    LoadHTML(html);
    ClearHandledUpdates();
    WebDocument document = GetMainFrame()->GetDocument();
    EXPECT_FALSE(document.IsNull());
    WebAXObject root_obj = WebAXObject::FromWebDocument(document);
    EXPECT_FALSE(root_obj.IsNull());
    SendPendingAccessibilityEvents();
  }

  void SetUp() override {
    blink::WebTestingSupport::SaveRuntimeFeatures();
    blink::WebRuntimeFeatures::EnableExperimentalFeatures(false);
    blink::WebRuntimeFeatures::EnableTestOnlyFeatures(false);
    blink::WebRuntimeFeatures::EnableAccessibilityExposeHTMLElement(true);

    RenderViewTest::SetUp();

    sink_ = &render_thread_->sink();

    // Ensure that a valid RenderAccessibilityImpl object is created and
    // associated to the RenderFrame, so that calls from tests to methods of
    // RenderAccessibilityImpl will work.
    frame()->SetAccessibilityModeForTest(ui::kAXModeWebContentsOnly.flags());
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
    // Do this before shutting down V8 in RenderViewTest::TearDown().
    // http://crbug.com/328552
    __lsan_do_leak_check();
#endif
    RenderViewTest::TearDown();
    blink::WebTestingSupport::ResetRuntimeFeatures();
  }

  void SetMode(ui::AXMode mode) {
    frame()->GetRenderAccessibilityManager()->SetMode(mode);
  }

  ui::AXTreeUpdate GetLastAccUpdate() {
    return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
        ->LastUpdate();
  }

  const std::vector<ui::AXTreeUpdate>& GetHandledAccUpdates() {
    return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
        ->HandledUpdates();
  }

  void ClearHandledUpdates() {
    return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
        ->ClearHandledUpdates();
  }

  std::vector<blink::mojom::LocationChangesPtr>& GetLocationChanges() {
    return static_cast<RenderAccessibilityTestRenderFrame*>(frame())
        ->LocationChanges();
  }

  int CountAccessibilityNodesSentToBrowser() {
    ui::AXTreeUpdate update = GetLastAccUpdate();
    return update.nodes.size();
  }

  // RenderFrameImpl::SendPendingAccessibilityEvents() is a protected method, so
  // we wrap it here and access it from tests via this friend class for testing.
  void SendPendingAccessibilityEvents() {
    // Ensure there are no pending events before sending accessibility events to
    // be able to properly check later on the nodes that have been updated, and
    // also wait for the mojo messages to be processed once they are sent.
    task_environment_.RunUntilIdle();
    GetRenderAccessibilityImpl()->ScheduleImmediateAXUpdate();
    GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
    task_environment_.RunUntilIdle();
  }

 private:
  IPC::TestSink* sink_;
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

  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());

  // If we post another event but the tree doesn't change,
  // we should only send 1 node to the browser.
  ClearHandledUpdates();
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj, false);
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
  LoadHTML(html);
  document = GetMainFrame()->GetDocument();
  root_obj = WebAXObject::FromWebDocument(document);
  ClearHandledUpdates();
  const WebAXObject& first_child = root_obj.ChildAt(0);
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(first_child.AxID(), ax::mojom::Event::kFocus));
  SendPendingAccessibilityEvents();
  EXPECT_EQ(6, CountAccessibilityNodesSentToBrowser());
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
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(node_a, false);

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
        <div role="group" id="A">
          <div role="group" id="B" style="visibility: hidden">
            <div role="group" id="C" style="visibility: visible">
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
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(node_a, false);
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
      std::string name;
      if (node.GetStringAttribute(ax::mojom::StringAttribute::kName, &name) &&
          name == "first") {
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
  std::vector<blink::mojom::LocationChangesPtr>& changes = GetLocationChanges();
  EXPECT_EQ(changes.size(), 1u);
  EXPECT_EQ(changes[0]->id, expected_id);
  EXPECT_EQ(changes[0]->new_location, expected_bounds);
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
      std::string name;
      node.GetStringAttribute(ax::mojom::StringAttribute::kName, &name);
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
  GetRenderAccessibilityImpl()->HandleAXEvent(
      ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kScrollPositionChanged));
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
  SendPendingAccessibilityEvents();

  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());

  // Make sure it's the root object that was updated for scrolling.
  ui::AXTreeUpdate update = GetLastAccUpdate();
  EXPECT_EQ(root_obj.AxID(), update.nodes[0].id);

  // Make sure that a location change is sent for the fixed-positioned node.
  std::vector<blink::mojom::LocationChangesPtr>& changes = GetLocationChanges();
  EXPECT_EQ(changes.size(), 2u);
  for (auto& change : changes) {
    auto search = expected.find(change->id);
    EXPECT_NE(search, expected.end());
    EXPECT_EQ(search->second, change->new_location);
  }
}

TEST_F(RenderAccessibilityImplTest, TestFocusConsistency) {
  constexpr char html[] = R"HTML(
      <body>
        <a id="link" tabindex=0>link</a>
        <button id="button" style="visibility:hidden" tabindex=0>button</button>
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

class MockPluginAccessibilityTreeSource : public content::PluginAXTreeSource {
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
  ui::AXNodeID root_node_id = GetRenderAccessibilityImpl()->GenerateAXID();
  MockPluginAccessibilityTreeSource pdf_acc_tree(root_node_id);
  GetRenderAccessibilityImpl()->SetPluginTreeSource(&pdf_acc_tree);

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
      AXActionTargetFactory::CreateFromNodeId(document, nullptr,
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

  // Increment/decrement actions produce synthesized keydown and keyup events,
  // and the keyup event is delayed 100ms to look more natural. We need to wait
  // for them to happen to finish the test cleanly in the TearDown phase.
  task_environment_.FastForwardBy(base::Seconds(1));

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

  // Android does not produce accessible items for option elements.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(IsSelected(option));
  EXPECT_TRUE(option_action_target->SetSelected(true));
  // Selecting option requires layout to be clean.
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
  EXPECT_TRUE(IsSelected(option));
#endif

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

//
// AXImageAnnotatorTest
//

class AXImageAnnotatorTest : public RenderAccessibilityImplTest {
 public:
  AXImageAnnotatorTest() = default;

  AXImageAnnotatorTest(const AXImageAnnotatorTest&) = delete;
  AXImageAnnotatorTest& operator=(const AXImageAnnotatorTest&) = delete;

  ~AXImageAnnotatorTest() override = default;

 protected:
  void SetUp() override {
    RenderAccessibilityImplTest::SetUp();
    // TODO(nektar): Add the ability to test the AX action that labels images
    // only once.
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    SetMode(mode);
    GetRenderAccessibilityImpl()->ax_image_annotator_ =
        std::make_unique<TestAXImageAnnotator>(GetRenderAccessibilityImpl(),
                                               mock_annotator().GetRemote());
    RenderAccessibilityImpl::IgnoreProtocolChecksForTesting();
  }

  void TearDown() override {
    GetRenderAccessibilityImpl()->ax_image_annotator_.release();
    RenderAccessibilityImplTest::TearDown();
  }

  MockAnnotationService& mock_annotator() { return mock_annotator_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockAnnotationService mock_annotator_;
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
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();
  ClearHandledUpdates();

  // This should update the annotations of all images on the page, including the
  // already visible one.
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj,
                                                     true /* subtree */);
  SendPendingAccessibilityEvents();
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

  ClearHandledUpdates();
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  ASSERT_FALSE(root_obj.IsNull());
  // This should update the annotations of all images on the page.
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj,
                                                     true /* subtree */);
  SendPendingAccessibilityEvents();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_,
              ElementsAre("test1.jpg", "test1.jpg"));
  ASSERT_EQ(2u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[1].is_bound());
  EXPECT_EQ(2u, mock_annotator().callbacks_.size());

  // Update node "A".
  ExecuteJavaScriptForTests("document.querySelector('img').src = 'test2.jpg';");
  GetRenderAccessibilityImpl()->GetAXContext()->UpdateAXForAllDocuments();

  ClearHandledUpdates();
  // This should update the annotations of all images on the page, including the
  // now updated image src.
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj,
                                                     true /* subtree */);
  SendPendingAccessibilityEvents();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator().image_ids_,
              ElementsAre("test1.jpg", "test1.jpg", "test2.jpg"));
  ASSERT_EQ(3u, mock_annotator().image_processors_.size());
  EXPECT_TRUE(mock_annotator().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator().image_processors_[2].is_bound());
  EXPECT_EQ(3u, mock_annotator().callbacks_.size());
}

// URL-keyed metrics recorder implementation that just counts the number
// of times it's been called.
class MockUkmRecorder : public ukm::MojoUkmRecorder {
 public:
  MockUkmRecorder()
      : ukm::MojoUkmRecorder(
            mojo::PendingRemote<ukm::mojom::UkmRecorderInterface>()) {}

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
    GetRenderAccessibilityImpl()->ukm_recorder_ =
        std::make_unique<MockUkmRecorder>();
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
      "Accessibility.Performance.SendPendingAccessibilityEvents", 0);

  // No URL-keyed metrics should be fired after we send one event.
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj, false);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(0, ukm_recorder()->calls());
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents", 1);

  // No URL-keyed metrics should be fired even after an event that takes
  // 300 ms, but we should now have something to send.
  // This must be >= kMinSerializationTimeToSendInMS
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj, false);
  SendPendingAccessibilityEvents();
  SetTimeDelayForNextSerialize(base::Milliseconds(300));
  EXPECT_EQ(0, ukm_recorder()->calls());
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents", 2);

  // After 1000 seconds have passed, the next time we send an event we should
  // send URL-keyed metrics.
  task_environment_.FastForwardBy(base::Seconds(1000));
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj, false);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(1, ukm_recorder()->calls());
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents", 3);

  // Send another event that takes a long (simulated) time to serialize.
  // This must be >= kMinSerializationTimeToSend
  SetTimeDelayForNextSerialize(base::Milliseconds(200));
  GetRenderAccessibilityImpl()->MarkWebAXObjectDirty(root_obj, false);
  SendPendingAccessibilityEvents();
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.SendPendingAccessibilityEvents", 4);

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
