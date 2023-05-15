// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "ui/events/blink/blink_event_util.h"

namespace blink {

LocalFrame* SingleChildLocalFrameClient::CreateFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {

  LocalFrame* parent_frame = owner_element->GetDocument().GetFrame();
  auto* child_client =
      MakeGarbageCollected<LocalFrameClientWithParent>(parent_frame);
  LocalFrame* child = MakeGarbageCollected<LocalFrame>(
      child_client, *parent_frame->GetPage(), owner_element, parent_frame,
      nullptr, FrameInsertType::kInsertInConstructor, LocalFrameToken(),
      &parent_frame->window_agent_factory(), nullptr);
  child->CreateView(gfx::Size(500, 500), Color::kTransparent);

  // The initial empty document's policy container is inherited from its parent.
  mojom::blink::PolicyContainerPoliciesPtr policy_container_data =
      parent_frame->GetDocument()
          ->GetExecutionContext()
          ->GetPolicyContainer()
          ->GetPolicies()
          .Clone();

  // The initial empty document's sandbox flags is further restricted by its
  // frame's sandbox attribute. At the end, it becomes the union of:
  // - The parent's sandbox flags.
  // - The iframe's sandbox attribute.
  policy_container_data->sandbox_flags |=
      child->Owner()->GetFramePolicy().sandbox_flags;

  // Create a dummy PolicyContainerHost remote. The messages are normally
  // handled by by the browser process, but they are dropped here.
  mojo::AssociatedRemote<mojom::blink::PolicyContainerHost> dummy_host;
  std::ignore = dummy_host.BindNewEndpointAndPassDedicatedReceiver();

  auto policy_container = std::make_unique<PolicyContainer>(
      dummy_host.Unbind(), std::move(policy_container_data));

  child->Init(/*opener=*/nullptr, DocumentToken(), std::move(policy_container),
              parent_frame->DomWindow()->GetStorageKey(),
              /*document_ukm_source_id=*/ukm::kInvalidSourceId,
              /*creator_base_url=*/KURL());

  return child;
}

void LocalFrameClientWithParent::Detached(FrameDetachType) {
  parent_->RemoveChild(parent_->FirstChild());
}

void RenderingTestChromeClient::InjectGestureScrollEvent(
    LocalFrame& local_frame,
    WebGestureDevice device,
    const gfx::Vector2dF& delta,
    ui::ScrollGranularity granularity,
    CompositorElementId scrollable_area_element_id,
    WebInputEvent::Type injected_type) {
  // Directly handle injected gesture scroll events. In a real browser, these
  // would be added to the event queue and handled asynchronously but immediate
  // handling is sufficient to test scrollbar dragging.
  std::unique_ptr<WebGestureEvent> gesture_event =
      WebGestureEvent::GenerateInjectedScrollGesture(
          injected_type, base::TimeTicks::Now(), device, gfx::PointF(0, 0),
          delta, granularity);
  if (injected_type == WebInputEvent::Type::kGestureScrollBegin) {
    gesture_event->data.scroll_begin.scrollable_area_element_id =
        scrollable_area_element_id.GetInternalValue();
  }
  local_frame.GetEventHandler().HandleGestureEvent(*gesture_event);
}

RenderingTestChromeClient& RenderingTest::GetChromeClient() const {
  DEFINE_STATIC_LOCAL(Persistent<RenderingTestChromeClient>, client,
                      (MakeGarbageCollected<RenderingTestChromeClient>()));
  return *client;
}

RenderingTest::RenderingTest(LocalFrameClient* local_frame_client)
    : local_frame_client_(local_frame_client) {}

const Node* RenderingTest::HitTest(int x, int y) {
  HitTestLocation location(PhysicalOffset(x, y));
  HitTestResult result(
      HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                     HitTestRequest::kAllowChildFrameContent),
      location);
  GetLayoutView().HitTest(location, result);
  return result.InnerNode();
}

HitTestResult::NodeSet RenderingTest::RectBasedHitTest(
    const PhysicalRect& rect) {
  HitTestLocation location(rect);
  HitTestResult result(
      HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                     HitTestRequest::kAllowChildFrameContent |
                     HitTestRequest::kListBased),
      location);
  GetLayoutView().HitTest(location, result);
  return result.ListBasedTestResult();
}

void RenderingTest::SetUp() {
  GetChromeClient().SetUp();
  SetupPageWithClients(&GetChromeClient(), local_frame_client_,
                       SettingOverrider());
  EXPECT_TRUE(
      GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());

  // This ensures that the minimal DOM tree gets attached
  // correctly for tests that don't call setBodyInnerHTML.
  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  UpdateAllLifecyclePhasesForTest();

  // Allow ASSERT_DEATH and EXPECT_DEATH for multiple threads.
  testing::FLAGS_gtest_death_test_style = "threadsafe";
}

void RenderingTest::TearDown() {
  // We need to destroy most of the Blink structure here because derived tests
  // may restore RuntimeEnabledFeatures setting during teardown, which happens
  // before our destructor getting invoked, breaking the assumption that REF
  // can't change during Blink lifetime.
  PageTestBase::TearDown();

  // Clear memory cache, otherwise we can leak pruned resources.
  MemoryCache::Get()->EvictResources();
}

void RenderingTest::SetChildFrameHTML(const String& html) {
  ChildDocument().SetBaseURLOverride(KURL("http://test.com"));
  ChildDocument().body()->setInnerHTML(html, ASSERT_NO_EXCEPTION);

  // Setting HTML implies the frame loads contents, so we need to advance the
  // state machine to leave the initial empty document state.
  ChildDocument().OverrideIsInitialEmptyDocument();
  // And let the frame view exit the initial throttled state.
  ChildDocument().View()->BeginLifecycleUpdates();
}

NGConstraintSpace RenderingTest::ConstraintSpaceForAvailableSize(
    LayoutUnit inline_size) const {
  NGConstraintSpaceBuilder builder(
      WritingMode::kHorizontalTb,
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      /* is_new_fc */ false);
  builder.SetAvailableSize(LogicalSize(inline_size, LayoutUnit::Max()));
  return builder.ToConstraintSpace();
}

}  // namespace blink
