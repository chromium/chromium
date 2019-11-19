// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

LocalFrame* SingleChildLocalFrameClient::CreateFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {
  DCHECK(!child_) << "This test helper only supports one child frame.";

  LocalFrame* parent_frame = owner_element->GetDocument().GetFrame();
  auto* child_client =
      MakeGarbageCollected<LocalFrameClientWithParent>(parent_frame);
  child_ = MakeGarbageCollected<LocalFrame>(
      child_client, *parent_frame->GetPage(), owner_element,
      &parent_frame->window_agent_factory(), nullptr);
  child_->CreateView(IntSize(500, 500), Color::kTransparent);
  child_->Init();

  return child_.Get();
}

void LocalFrameClientWithParent::Detached(FrameDetachType) {
  static_cast<SingleChildLocalFrameClient*>(Parent()->Client())
      ->DidDetachChild();
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
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  GetChromeClient().SetUp();
  page_clients.chrome_client = &GetChromeClient();
  SetupPageWithClients(&page_clients, local_frame_client_, SettingOverrider());
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
  GetMemoryCache()->EvictResources();
}

void RenderingTest::SetChildFrameHTML(const String& html) {
  ChildDocument().SetBaseURLOverride(KURL("http://test.com"));
  ChildDocument().body()->SetInnerHTMLFromString(html, ASSERT_NO_EXCEPTION);

  // Setting HTML implies the frame loads contents, so we need to advance the
  // state machine to leave the initial empty document state.
  auto* state_machine = ChildDocument().GetFrame()->Loader().StateMachine();
  if (state_machine->IsDisplayingInitialEmptyDocument())
    state_machine->AdvanceTo(FrameLoaderStateMachine::kCommittedFirstRealLoad);
  // And let the frame view exit the initial throttled state.
  ChildDocument().View()->BeginLifecycleUpdates();
}

}  // namespace blink
