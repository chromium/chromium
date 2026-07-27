// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {
std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<EventRouter>(context, ExtensionPrefs::Get(context));
}

class TestEventDispatchTrackingObserver : public EventRouter::TestObserver {
 public:
  TestEventDispatchTrackingObserver() = default;
  ~TestEventDispatchTrackingObserver() override = default;

  void OnWillDispatchEvent(const Event& event) override {}

  void OnDidDispatchEventToProcess(const Event& event,
                                   int process_id) override {}

  void OnWillBroadcastEvent(const Event& event) override {
    broadcast_events_.push_back(event.event_name);
  }

  const std::vector<std::string>& broadcast_events() const {
    return broadcast_events_;
  }

 private:
  std::vector<std::string> broadcast_events_;
};
}  // namespace

class TestStartedAndFinishedEventQueueUnitTest : public ApiUnitTest {
 public:
  TestStartedAndFinishedEventQueueUnitTest() = default;
  ~TestStartedAndFinishedEventQueueUnitTest() override = default;

  void SetUp() override {
    ApiUnitTest::SetUp();
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(), base::BindRepeating(&BuildEventRouter));
  }
};

// Verifies that when an event listener is added but no pending events exist
// in the queue for `details.event_name`, `OnListenerAdded()` returns
// immediately as a no-op without dispatching any events or mutating queue
// state.
TEST_F(TestStartedAndFinishedEventQueueUnitTest, NoPendingEventsNoOp) {
  TestStartedAndFinishedEventQueue* queue =
      TestStartedAndFinishedEventQueue::Get(browser_context());
  ASSERT_TRUE(queue);

  // Observe event dispatch so we can verify no events are dispatched.
  EventRouter* event_router = EventRouter::Get(browser_context());
  TestEventDispatchTrackingObserver observer;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(event_router);

  // Confirm that the queue is empty before adding any listener.
  ASSERT_TRUE(queue->IsQueueEmptyForTesting());

  // Simulate registering an event listener for `chrome.test.onTestStarted` when
  // no test events have been queued.
  auto event_target_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  EventListenerInfo listener_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_target_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(listener_details);

  // Verify that no events were broadcast and the queue remains empty.
  EXPECT_TRUE(observer.broadcast_events().empty());
  EXPECT_TRUE(queue->IsQueueEmptyForTesting());
}

// Verifies that when multiple test events (`chrome.test.onTestStarted` or
// `chrome.test.onTestFinished`) are enqueued, they are dispatched in exact
// First-In, First-Out (FIFO) order to `EventRouter::BroadcastEvent()` when a
// listener registers from a different process, and the queue drains completely.
TEST_F(TestStartedAndFinishedEventQueueUnitTest, MultipleEventsFifoDispatch) {
  TestStartedAndFinishedEventQueue* queue =
      TestStartedAndFinishedEventQueue::Get(browser_context());
  ASSERT_TRUE(queue);

  // Observe event dispatch so we can verify the order of dispatched events.
  EventRouter* event_router = EventRouter::Get(browser_context());
  TestEventDispatchTrackingObserver observer;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(event_router);

  // Enqueue two `chrome.test.onTestStarted` events and one
  // `chrome.test.onTestFinished` event from a source renderer process.
  auto event_source_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  base::ListValue first_started_args;
  first_started_args.Append("testOne");
  queue->EnqueueEvent(api::test::OnTestStarted::kEventName,
                      std::move(first_started_args),
                      event_source_process->GetID());

  base::ListValue second_started_args;
  second_started_args.Append("testTwo");
  queue->EnqueueEvent(api::test::OnTestStarted::kEventName,
                      std::move(second_started_args),
                      event_source_process->GetID());

  base::ListValue finished_args;
  finished_args.Append("testOne");
  finished_args.Append(true);
  queue->EnqueueEvent(api::test::OnTestFinished::kEventName,
                      std::move(finished_args), event_source_process->GetID());

  // Verify the pending events count reflects all queued events.
  EXPECT_EQ(2u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestStarted::kEventName));
  EXPECT_EQ(1u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestFinished::kEventName));

  // Register an event listener for `chrome.test.onTestStarted` from a different
  // renderer process.
  auto event_target_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  EventListenerInfo started_listener_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_target_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(started_listener_details);

  // Verify that the queued `chrome.test.onTestStarted` events were dispatched
  // in FIFO order and that the queue entry is empty.
  EXPECT_EQ(0u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestStarted::kEventName));
  ASSERT_EQ(2u, observer.broadcast_events().size());
  EXPECT_EQ(api::test::OnTestStarted::kEventName,
            observer.broadcast_events()[0]);
  EXPECT_EQ(api::test::OnTestStarted::kEventName,
            observer.broadcast_events()[1]);

  // Register an event listener for `chrome.test.onTestFinished` from the same
  // target renderer process.
  EventListenerInfo finished_listener_details(
      api::test::OnTestFinished::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_target_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(finished_listener_details);

  // Verify that the queued `chrome.test.onTestFinished` event was dispatched
  // and the queue is completely empty.
  EXPECT_TRUE(queue->IsQueueEmptyForTesting());
  ASSERT_EQ(3u, observer.broadcast_events().size());
  EXPECT_EQ(api::test::OnTestFinished::kEventName,
            observer.broadcast_events()[2]);
}

// Verifies that when a queued test event originates from a specific
// renderer process, registering a new event listener within that same process
// does not consume the event. The event should be retained in the
// queue so that later a new event listener in a different process can
// receive it.
TEST_F(TestStartedAndFinishedEventQueueUnitTest, RemainingEventsRetention) {
  TestStartedAndFinishedEventQueue* queue =
      TestStartedAndFinishedEventQueue::Get(browser_context());
  ASSERT_TRUE(queue);

  // Observe event dispatch so we can verify it later.
  EventRouter* event_router = EventRouter::Get(browser_context());
  TestEventDispatchTrackingObserver observer;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(event_router);

  // Example args for `chrome.test.onTestStarted`.
  base::ListValue event_args;
  event_args.Append("myTestName");

  // Enqueue a test event from a renderer process.
  auto event_source_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  queue->EnqueueEvent(api::test::OnTestStarted::kEventName,
                      std::move(event_args), event_source_process->GetID());
  EXPECT_EQ(1u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestStarted::kEventName));

  // Simulate registering an event listener from within the exact same process.
  // Because the listener resides in the emitting process, the event will
  // remain in the queue.
  EventListenerInfo same_process_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_source_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(same_process_details);
  EXPECT_EQ(1u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestStarted::kEventName));
  EXPECT_TRUE(observer.broadcast_events().empty());

  // Simulate registering an event listener from a different renderer process.
  // The queued event is now dispatched across process boundaries and the
  // event queue drains completely.
  auto event_target_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  EventListenerInfo different_process_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_target_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(different_process_details);
  EXPECT_TRUE(queue->IsQueueEmptyForTesting());
  ASSERT_EQ(1u, observer.broadcast_events().size());
  EXPECT_EQ(api::test::OnTestStarted::kEventName,
            observer.broadcast_events()[0]);
}

// Verifies that when an event listener for `chrome.test.onTestStarted` is
// registered in the same process before the test event is emitted,
// `BroadcastOrQueueTestNotificationForTesting()` still enqueues the event
// because no listeners exist outside that source process. When a listener
// subsequently registers from a different process, the queued event is
// dispatched to that new process.
TEST_F(TestStartedAndFinishedEventQueueUnitTest,
       PreExistingSameProcessListenerQueuing) {
  TestStartedAndFinishedEventQueue* queue =
      TestStartedAndFinishedEventQueue::Get(browser_context());
  ASSERT_TRUE(queue);

  // Observe event dispatch so we can verify it later.
  EventRouter* event_router = EventRouter::Get(browser_context());
  TestEventDispatchTrackingObserver observer;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(event_router);

  auto event_source_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  // Register an event listener inside `event_source_process` before any
  // test event is emitted.
  EventListenerInfo same_process_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_source_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  event_router->AddEventListenerForTesting(same_process_details.event_name,
                                           event_source_process.get(),
                                           same_process_details.extension_id);

  // Emit a test started event from `event_source_process`.
  base::ListValue event_args;
  event_args.Append("myTestName");
  BroadcastOrQueueTestNotificationForTesting(
      browser_context(), api::test::OnTestStarted::kEventName,
      std::move(event_args), event_source_process->GetID());

  // Because the only registered listener belongs to
  // `event_source_process`, the event should be queued in
  // `pending_events_` rather than dropped or broadcast.
  EXPECT_EQ(1u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestStarted::kEventName));
  EXPECT_TRUE(observer.broadcast_events().empty());

  // Register a listener in a different renderer process. The queued event
  // should now be dispatched to this new listener.
  auto event_target_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());
  EventListenerInfo different_process_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), event_target_process->GetID(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(different_process_details);

  EXPECT_TRUE(queue->IsQueueEmptyForTesting());
  ASSERT_EQ(1u, observer.broadcast_events().size());
  EXPECT_EQ(api::test::OnTestStarted::kEventName,
            observer.broadcast_events()[0]);
}

// Verifies that when an event listener registers from a valid process ID with
// an empty extension ID and non-empty target URL (such as a WebUI or web page
// observing `chrome.test`), queued events for that event are dispatched
// globally via `EventRouter::BroadcastEvent()`.
TEST_F(TestStartedAndFinishedEventQueueUnitTest, URLListenerDispatch) {
  TestStartedAndFinishedEventQueue* queue =
      TestStartedAndFinishedEventQueue::Get(browser_context());
  ASSERT_TRUE(queue);

  EventRouter* event_router = EventRouter::Get(browser_context());
  TestEventDispatchTrackingObserver observer;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(event_router);

  auto event_target_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context());

  base::ListValue event_args;
  event_args.Append("myTestName");

  queue->EnqueueEvent(api::test::OnTestStarted::kEventName,
                      std::move(event_args),
                      content::ChildProcessId::FromUnsafeValue(100));

  const GURL listener_url("chrome://webui/test.html");
  EventListenerInfo url_listener_details(
      api::test::OnTestStarted::kEventName, /*extension_id=*/std::string(),
      listener_url, /*filter=*/nullptr, browser_context(),
      event_target_process->GetID(),
      /*worker_thread_id=*/0, /*service_worker_version_id=*/0,
      /*is_lazy=*/false);
  queue->OnListenerAdded(url_listener_details);

  EXPECT_TRUE(queue->IsQueueEmptyForTesting());
  ASSERT_EQ(1u, observer.broadcast_events().size());
  EXPECT_EQ(api::test::OnTestStarted::kEventName,
            observer.broadcast_events()[0]);
}

// Verifies that when both the queued event and the registering event
// listener have null or invalid process IDs, the event is not falsely matched
// as originating from the same process and falls back to being broadcast
// globally across all processes.
TEST_F(TestStartedAndFinishedEventQueueUnitTest, InvalidProcessFallback) {
  TestStartedAndFinishedEventQueue* queue =
      TestStartedAndFinishedEventQueue::Get(browser_context());
  ASSERT_TRUE(queue);

  // Observe event dispatch so we can verify it later.
  EventRouter* event_router = EventRouter::Get(browser_context());
  TestEventDispatchTrackingObserver observer;
  base::ScopedObservation<EventRouter, EventRouter::TestObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(event_router);

  // Example args for `chrome.test.onTestStarted`.
  base::ListValue event_args;
  event_args.Append("myTestName");

  // Enqueue a test event with a null `ChildProcessId`.
  queue->EnqueueEvent(api::test::OnTestStarted::kEventName,
                      std::move(event_args), content::ChildProcessId());
  EXPECT_EQ(1u, queue->GetPendingEventsCountForTesting(
                    api::test::OnTestStarted::kEventName));

  // Simulate registering an event listener without an associated process ID.
  EventListenerInfo invalid_process_details(
      api::test::OnTestStarted::kEventName, "extension_id",
      /*listener_url=*/GURL(),
      /*filter=*/nullptr, browser_context(), content::ChildProcessId(),
      /*worker_thread_id=*/0,
      /*service_worker_version_id=*/0, /*is_lazy=*/false);
  queue->OnListenerAdded(invalid_process_details);

  // Verify that the event falls back to being broadcast globally across
  // all processes, and the event queue drains completely.
  EXPECT_TRUE(queue->IsQueueEmptyForTesting());
  ASSERT_EQ(1u, observer.broadcast_events().size());
  const std::string& broadcast_event = observer.broadcast_events()[0];
  EXPECT_EQ(api::test::OnTestStarted::kEventName, broadcast_event);
}

}  // namespace extensions
