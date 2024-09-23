// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_context.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/origin.h"

namespace content {

namespace {
constexpr char kTestUrl[] = "https://google.com";
}

class TestPermissionObserver : public blink::mojom::PermissionObserver {
 public:
  TestPermissionObserver() = default;

  TestPermissionObserver(const TestPermissionObserver&) = delete;
  TestPermissionObserver& operator=(const TestPermissionObserver&) = delete;

  ~TestPermissionObserver() override = default;

  // Closes the bindings associated with this observer.
  void Close() { receiver_.reset(); }

  // Returns a pipe to this observer.
  mojo::PendingRemote<blink::mojom::PermissionObserver> GetRemote() {
    mojo::PendingRemote<blink::mojom::PermissionObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // Returns the number of events received by this observer.
  size_t change_event_count() const { return change_event_count_; }

  // blink::mojom::PermissionObserver implementation.
  void OnPermissionStatusChange(
      blink::mojom::PermissionStatus status) override {
    change_event_count_++;
  }

 private:
  size_t change_event_count_ = 0;
  mojo::Receiver<blink::mojom::PermissionObserver> receiver_{this};
};

class PermissionServiceContextTest : public RenderViewHostTestHarness {
 public:
  PermissionServiceContextTest() = default;
  PermissionServiceContextTest(const PermissionServiceContextTest&) = delete;
  PermissionServiceContextTest& operator=(const PermissionServiceContextTest&) =
      delete;
  ~PermissionServiceContextTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    origin_ = url::Origin::Create(GURL(kTestUrl));
    NavigateAndCommit(origin_.GetURL());
    permission_controller_ =
        PermissionControllerImpl::FromBrowserContext(browser_context());
    auto* render_frame_host = main_rfh();
    render_frame_host_impl_ =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    permission_service_context_ =
        PermissionServiceContext::GetOrCreateForCurrentDocument(
            render_frame_host);
  }

  void TearDown() override {
    permission_controller_ = nullptr;
    render_frame_host_impl_ = nullptr;
    permission_service_context_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<TestPermissionObserver> CreateSubscription(
      PermissionType type,
      blink::mojom::PermissionStatus last_status,
      blink::mojom::PermissionStatus current_status) {
    permission_controller()->SetOverrideForDevTools(origin_, type, last_status);
    auto observer = std::make_unique<TestPermissionObserver>();
    permission_service_context()->CreateSubscription(
        type, origin_, current_status, last_status,
        /*should_include_device_status=*/false, observer->GetRemote());
    WaitForAsyncTasksToComplete();
    return observer;
  }

  void SimulatePermissionChangedEvent(PermissionType type,
                                      blink::mojom::PermissionStatus status) {
    permission_controller()->SetOverrideForDevTools(origin_, type, status);
    WaitForAsyncTasksToComplete();
  }

  // Waits until the Mojo task (async) has finished.
  void WaitForAsyncTasksToComplete() { task_environment()->RunUntilIdle(); }

  PermissionControllerImpl* permission_controller() {
    return permission_controller_;
  }

  PermissionServiceContext* permission_service_context() {
    return permission_service_context_;
  }

  RenderFrameHostImpl* render_frame_host() { return render_frame_host_impl_; }

 private:
  url::Origin origin_;
  raw_ptr<PermissionControllerImpl> permission_controller_;
  raw_ptr<RenderFrameHostImpl> render_frame_host_impl_;
  raw_ptr<PermissionServiceContext> permission_service_context_;
};

TEST_F(PermissionServiceContextTest, DispatchPermissionChangeEvent) {
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  auto observer = CreateSubscription(PermissionType::GEOLOCATION,
                                     blink::mojom::PermissionStatus::ASK,
                                     blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);
  EXPECT_EQ(observer->change_event_count(), 1U);
}

TEST_F(PermissionServiceContextTest,
       DispatchPermissionChangeEventInBackForwardCache) {
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  auto observer = CreateSubscription(PermissionType::GEOLOCATION,
                                     blink::mojom::PermissionStatus::ASK,
                                     blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);

  // After dispatching changed events when the render frame host is active,
  // the event counter should increment as expected.
  EXPECT_EQ(observer->change_event_count(), 1U);

  // Same origin child sub-frame should also receive changed events but should
  // not double increment the parent's counter.
  RenderFrameHost* child =
      RenderFrameHostTester::For(render_frame_host())->AppendChild("");
  RenderFrameHostTester::For(child)->InitializeRenderFrameIfNeeded();
  auto navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl),
                                                            child);
  navigation_simulator->Commit();
  child = navigation_simulator->GetFinalRenderFrameHost();
  auto* permission_service_context =
      PermissionServiceContext::GetOrCreateForCurrentDocument(child);
  auto observer_child = std::make_unique<TestPermissionObserver>();
  permission_service_context->CreateSubscription(
      PermissionType::GEOLOCATION, url::Origin::Create(GURL(kTestUrl)),
      blink::mojom::PermissionStatus::ASK, blink::mojom::PermissionStatus::ASK,
      /*should_include_device_status=*/false, observer_child->GetRemote());
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 2U);
  EXPECT_EQ(observer_child->change_event_count(), 1U);

  // Simulate the render frame host is put into the back/forward cache
  render_frame_host()->DidEnterBackForwardCache();
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);

  // Trigger a permission status change event.
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::ASK);

  // Now the change events should not should increment the counter.
  EXPECT_EQ(observer->change_event_count(), 2U);
  EXPECT_EQ(observer_child->change_event_count(), 1U);

  // Simulate the render frame host is back to active state by setting the
  // lifecycle state.
  render_frame_host()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);

  // Since the render frame host is active, the dispatched events should
  // increment the counter.
  EXPECT_EQ(observer->change_event_count(), 3U);
  EXPECT_EQ(observer_child->change_event_count(), 2U);
}

TEST_F(PermissionServiceContextTest,
       DispatchMultiplePermissionChangeEventsInBackForwardCache) {
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  auto observer = CreateSubscription(PermissionType::GEOLOCATION,
                                     blink::mojom::PermissionStatus::ASK,
                                     blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Simulate the render frame host is put into the back/forward cache.
  // Trigger a permission status change event, the event should not should
  // increment the counter.
  render_frame_host()->DidEnterBackForwardCache();
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));

  for (size_t i = 0; i < 10; ++i) {
    SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                   blink::mojom::PermissionStatus::ASK);
    EXPECT_EQ(observer->change_event_count(), 0U);
    SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                   blink::mojom::PermissionStatus::DENIED);
    EXPECT_EQ(observer->change_event_count(), 0U);
  }

  // Simulate the render frame host is back to active state by setting the
  // lifecycle state. The last event should be dispatched and increment the
  // counter.
  render_frame_host()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  WaitForAsyncTasksToComplete();
  EXPECT_EQ(observer->change_event_count(), 1U);
}

TEST_F(PermissionServiceContextTest, CreateSubscriptionInBackForwardCache) {
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  render_frame_host()->DidEnterBackForwardCache();
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));

  // Create a subscription in BFCache
  auto observer = CreateSubscription(PermissionType::GEOLOCATION,
                                     blink::mojom::PermissionStatus::ASK,
                                     blink::mojom::PermissionStatus::ASK);
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);
  EXPECT_EQ(observer->change_event_count(), 0U);
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::GRANTED);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Simulate the render frame host is back to active state by setting the
  // lifecycle state. The last event should be dispatched.
  render_frame_host()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  WaitForAsyncTasksToComplete();
  EXPECT_EQ(observer->change_event_count(), 1U);
}

TEST_F(PermissionServiceContextTest,
       DispatchSameStatusAfterLeaveBackForwardCache) {
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  auto observer = CreateSubscription(PermissionType::GEOLOCATION,
                                     blink::mojom::PermissionStatus::ASK,
                                     blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Simulate the render frame host is put into the back/forward cache.
  // Trigger a permission status change event, the event should not should
  // increment the counter.
  render_frame_host()->DidEnterBackForwardCache();
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Permission status changes back to the status at BFCache entry
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Simulate the render frame host is back to active state by setting the
  // lifecycle state. No event should be dispatched.
  render_frame_host()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  WaitForAsyncTasksToComplete();
  EXPECT_EQ(observer->change_event_count(), 0U);
}

TEST_F(PermissionServiceContextTest,
       DispatchDifferentStatusAfterLeaveBackForwardCache) {
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  auto observer = CreateSubscription(PermissionType::GEOLOCATION,
                                     blink::mojom::PermissionStatus::ASK,
                                     blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Simulate the render frame host is put into the back/forward cache.
  // Trigger a permission status change event, the event should not should
  // increment the counter.
  render_frame_host()->DidEnterBackForwardCache();
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));
  SimulatePermissionChangedEvent(blink::PermissionType::GEOLOCATION,
                                 blink::mojom::PermissionStatus::DENIED);
  EXPECT_EQ(observer->change_event_count(), 0U);

  // Simulate the render frame host is back to active state by setting the
  // lifecycle state. The last event should be dispatched.
  render_frame_host()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kActive);
  EXPECT_TRUE(render_frame_host()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kActive));
  WaitForAsyncTasksToComplete();
  EXPECT_EQ(observer->change_event_count(), 1U);
}

}  // namespace content
