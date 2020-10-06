// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_event_queue.h"

#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class MockEvent {
 public:
  MockEvent() {}

  int event_id() const {
    DCHECK(event_id_.has_value());
    return *event_id_;
  }

  const base::Optional<mojom::blink::ServiceWorkerEventStatus>& status() const {
    return status_;
  }

  bool Started() const { return event_id_.has_value(); }

  void EnqueueTo(ServiceWorkerEventQueue* event_queue) {
    event_queue->EnqueueNormal(
        WTF::Bind(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        WTF::Bind(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        base::nullopt);
  }

  void EnqueuePendingTo(ServiceWorkerEventQueue* event_queue) {
    event_queue->EnqueuePending(
        WTF::Bind(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        WTF::Bind(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        base::nullopt);
  }

  void EnqueueWithCustomTimeoutTo(ServiceWorkerEventQueue* event_queue,
                                  base::TimeDelta custom_timeout) {
    event_queue->EnqueueNormal(
        WTF::Bind(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        WTF::Bind(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        custom_timeout);
  }

  void EnqueueOfflineTo(ServiceWorkerEventQueue* event_queue) {
    event_queue->EnqueueOffline(
        WTF::Bind(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        WTF::Bind(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        base::nullopt);
  }

  void EnqueuePendingDispatchingEventTo(ServiceWorkerEventQueue* event_queue,
                                        String tag,
                                        Vector<String>* out_tags) {
    event_queue->EnqueuePending(
        WTF::Bind(
            [](ServiceWorkerEventQueue* event_queue, MockEvent* event,
               String tag, Vector<String>* out_tags, int /* event id */) {
              event->EnqueueTo(event_queue);
              EXPECT_FALSE(event_queue->did_idle_timeout());
              // Event dispatched inside of a pending event should not run
              // immediately.
              EXPECT_FALSE(event->Started());
              EXPECT_FALSE(event->status().has_value());
              out_tags->emplace_back(std::move(tag));
            },
            WTF::Unretained(event_queue), WTF::Unretained(this), std::move(tag),
            WTF::Unretained(out_tags)),
        base::DoNothing(), base::nullopt);
  }

 private:
  void Start(int event_id) {
    EXPECT_FALSE(Started());
    event_id_ = event_id;
  }

  void Abort(int event_id, mojom::blink::ServiceWorkerEventStatus status) {
    EXPECT_EQ(event_id_, event_id);
    EXPECT_FALSE(status_.has_value());
    status_ = status;
  }

  base::Optional<int> event_id_;
  base::Optional<mojom::blink::ServiceWorkerEventStatus> status_;
  base::WeakPtrFactory<MockEvent> weak_factory_{this};
};

base::RepeatingClosure CreateReceiverWithCalledFlag(bool* out_is_called) {
  return WTF::BindRepeating([](bool* out_is_called) { *out_is_called = true; },
                            WTF::Unretained(out_is_called));
}

}  // namespace

using StayAwakeToken = ServiceWorkerEventQueue::StayAwakeToken;

class ServiceWorkerEventQueueTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    // Ensure all things run on |task_runner_| instead of the default task
    // runner initialized by blink_unittests.
    task_runner_context_ =
        std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
            task_runner_);
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext>
      task_runner_context_;
};

TEST_F(ServiceWorkerEventQueueTest, IdleTimer) {
  const base::TimeDelta kIdleInterval = base::TimeDelta::FromSeconds(
      mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds);

  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(
      base::NullCallback(), CreateReceiverWithCalledFlag(&is_idle),
      task_runner(), task_runner()->GetMockTickClock());
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing should happen since the event queue has not started yet.
  EXPECT_FALSE(is_idle);

  event_queue.Start();
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired since there is no event.
  EXPECT_TRUE(is_idle);

  is_idle = false;
  MockEvent event1;
  event1.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  MockEvent event2;
  event2.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there are two inflight events.
  EXPECT_FALSE(is_idle);

  event_queue.EndEvent(event2.event_id());
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  event_queue.EndEvent(event1.event_id());
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired.
  EXPECT_TRUE(is_idle);

  is_idle = false;
  MockEvent event3;
  event3.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  std::unique_ptr<StayAwakeToken> token = event_queue.CreateStayAwakeToken();
  event_queue.EndEvent(event3.event_id());
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is a living StayAwakeToken.
  EXPECT_FALSE(is_idle);

  token.reset();
  // |idle_callback| isn't triggered immendiately.
  EXPECT_FALSE(is_idle);
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired.
  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerEventQueueTest, InflightEventBeforeStart) {
  const base::TimeDelta kIdleInterval = base::TimeDelta::FromSeconds(
      mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds);

  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(
      base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle), task_runner(),
      task_runner()->GetMockTickClock());
  MockEvent event;
  event.EnqueueTo(&event_queue);
  event_queue.Start();
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);
}

// Tests whether idle_time_ won't be updated in Start() when there was an
// event. The timeline is something like:
// [StartEvent] [EndEvent]
//       +----------+
//                  ^
//                  +-- idle_time_ --+
//                                   v
//                           [TimerStart]         [UpdateStatus]
//                                 +-- kUpdateInterval --+
// In the first UpdateStatus() the idle callback should be triggered.
TEST_F(ServiceWorkerEventQueueTest, EventFinishedBeforeStart) {
  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(
      base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle), task_runner(),
      task_runner()->GetMockTickClock());
  // Start and finish an event before starting the timer.
  MockEvent event;
  event.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  event_queue.EndEvent(event.event_id());

  // Move the time ticks to almost before |idle_time_| so that |idle_callback|
  // will get called at the first update check.
  task_runner()->FastForwardBy(
      base::TimeDelta::FromSeconds(
          mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds) -
      base::TimeDelta::FromSeconds(1));

  event_queue.Start();

  // Make sure the timer calls UpdateStatus().
  task_runner()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // |idle_callback| should be fired because enough time passed since the last
  // event.
  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerEventQueueTest, EventTimer) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), base::DoNothing(),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();

  MockEvent event1, event2;
  event1.EnqueueTo(&event_queue);
  event2.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(event1.status().has_value());
  EXPECT_FALSE(event2.status().has_value());
  event_queue.EndEvent(event1.event_id());
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kEventTimeout +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(event1.status().has_value());
  EXPECT_TRUE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event2.status().value());
}

TEST_F(ServiceWorkerEventQueueTest, CustomTimeouts) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), base::DoNothing(),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  MockEvent event1, event2;
  event1.EnqueueWithCustomTimeoutTo(&event_queue,
                                    ServiceWorkerEventQueue::kUpdateInterval -
                                        base::TimeDelta::FromSeconds(1));
  event2.EnqueueWithCustomTimeoutTo(
      &event_queue, ServiceWorkerEventQueue::kUpdateInterval * 2 -
                        base::TimeDelta::FromSeconds(1));
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(event1.status().has_value());
  EXPECT_FALSE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event1.status().value());
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(event1.status().has_value());
  EXPECT_TRUE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event2.status().value());
}

TEST_F(ServiceWorkerEventQueueTest, BecomeIdleAfterAbort) {
  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(
      base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle), task_runner(),
      task_runner()->GetMockTickClock());
  event_queue.Start();

  MockEvent event;
  event.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kEventTimeout +
                               ServiceWorkerEventQueue::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  // |event| should have been aborted, and at the same time, the idle timeout
  // should also be fired since there has been an aborted event.
  EXPECT_TRUE(event.status().has_value());
  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerEventQueueTest, AbortAllOnDestruction) {
  MockEvent event1, event2;
  {
    ServiceWorkerEventQueue event_queue(base::DoNothing(), base::DoNothing(),
                                        task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();

    event1.EnqueueTo(&event_queue);
    event2.EnqueueTo(&event_queue);

    task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    EXPECT_FALSE(event1.status().has_value());
    EXPECT_FALSE(event2.status().has_value());
  }

  EXPECT_TRUE(event1.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::ABORTED,
            event1.status().value());
  EXPECT_TRUE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::ABORTED,
            event2.status().value());
}

TEST_F(ServiceWorkerEventQueueTest, PushPendingTask) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), base::DoNothing(),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  task_runner()->FastForwardBy(base::TimeDelta::FromSeconds(
      mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds));
  EXPECT_TRUE(event_queue.did_idle_timeout());

  MockEvent pending_event;
  pending_event.EnqueuePendingTo(&event_queue);
  EXPECT_FALSE(pending_event.Started());

  // Start a new event. PushTask() should run the pending tasks.
  MockEvent event;
  event.EnqueueTo(&event_queue);
  EXPECT_FALSE(event_queue.did_idle_timeout());
  EXPECT_TRUE(pending_event.Started());
}

// Test that pending tasks are run when StartEvent() is called while there the
// idle event_queue.delay is zero. Regression test for https://crbug.com/878608.
TEST_F(ServiceWorkerEventQueueTest, RunPendingTasksWithZeroIdleTimerDelay) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), base::DoNothing(),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  event_queue.SetIdleDelay(base::TimeDelta::FromSeconds(0));
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(event_queue.did_idle_timeout());

  MockEvent event1, event2;
  Vector<String> handled_tasks;
  event1.EnqueuePendingDispatchingEventTo(&event_queue, "1", &handled_tasks);
  event2.EnqueuePendingDispatchingEventTo(&event_queue, "2", &handled_tasks);
  EXPECT_TRUE(handled_tasks.IsEmpty());

  // Start a new event. PushTask() should run the pending tasks.
  MockEvent event;
  event.EnqueueTo(&event_queue);
  EXPECT_FALSE(event_queue.did_idle_timeout());
  ASSERT_EQ(2u, handled_tasks.size());
  EXPECT_EQ("1", handled_tasks[0]);
  EXPECT_EQ("2", handled_tasks[1]);
  // Events dispatched inside of a pending task should run.
  EXPECT_TRUE(event1.Started());
  EXPECT_TRUE(event2.Started());
}

TEST_F(ServiceWorkerEventQueueTest, SetIdleTimerDelayToZero) {
  {
    bool is_idle = false;
    ServiceWorkerEventQueue event_queue(
        base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle),
        task_runner(), task_runner()->GetMockTickClock());
    event_queue.Start();
    EXPECT_FALSE(is_idle);

    event_queue.SetIdleDelay(base::TimeDelta::FromSeconds(0));
    task_runner()->RunUntilIdle();
    // |idle_callback| should be fired since there is no event.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerEventQueue event_queue(
        base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle),
        task_runner(), task_runner()->GetMockTickClock());
    event_queue.Start();
    MockEvent event;
    event.EnqueueTo(&event_queue);
    event_queue.SetIdleDelay(base::TimeDelta::FromSeconds(0));
    task_runner()->RunUntilIdle();
    // Nothing happens since there is an inflight event.
    EXPECT_FALSE(is_idle);

    event_queue.EndEvent(event.event_id());
    task_runner()->RunUntilIdle();
    // EndEvent() immediately triggers the idle callback.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerEventQueue event_queue(
        base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle),
        task_runner(), task_runner()->GetMockTickClock());
    event_queue.Start();
    MockEvent event1, event2;
    event1.EnqueueTo(&event_queue);
    event2.EnqueueTo(&event_queue);
    event_queue.SetIdleDelay(base::TimeDelta::FromSeconds(0));
    task_runner()->RunUntilIdle();
    // Nothing happens since there are two inflight events.
    EXPECT_FALSE(is_idle);

    event_queue.EndEvent(event1.event_id());
    task_runner()->RunUntilIdle();
    // Nothing happens since there is an inflight event.
    EXPECT_FALSE(is_idle);

    event_queue.EndEvent(event2.event_id());
    task_runner()->RunUntilIdle();
    // EndEvent() immediately triggers the idle callback when no inflight events
    // exist.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerEventQueue event_queue(
        base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle),
        task_runner(), task_runner()->GetMockTickClock());
    event_queue.Start();
    std::unique_ptr<StayAwakeToken> token_1 =
        event_queue.CreateStayAwakeToken();
    std::unique_ptr<StayAwakeToken> token_2 =
        event_queue.CreateStayAwakeToken();
    event_queue.SetIdleDelay(base::TimeDelta::FromSeconds(0));
    task_runner()->RunUntilIdle();
    // Nothing happens since there are two living tokens.
    EXPECT_FALSE(is_idle);

    token_1.reset();
    task_runner()->RunUntilIdle();
    // Nothing happens since there is an living token.
    EXPECT_FALSE(is_idle);

    token_2.reset();
    task_runner()->RunUntilIdle();
    // EndEvent() immediately triggers the idle callback when no tokens exist.
    EXPECT_TRUE(is_idle);
  }
}

TEST_F(ServiceWorkerEventQueueTest, EnqueuOffline) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), base::DoNothing(),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();

  MockEvent event_1;
  event_1.EnqueueTo(&event_queue);
  // State:
  // - inflight_events: {1 (normal)}
  // - queue: []
  EXPECT_TRUE(event_1.Started());

  MockEvent event_2;
  event_2.EnqueueTo(&event_queue);
  // |event_queue| should start |event_2| because both 1 and 2 are normal
  // events.
  //
  // State:
  // - inflight_events: {1 (normal), 2 (normal)}
  // - queue: []
  EXPECT_TRUE(event_2.Started());

  MockEvent event_3;
  event_3.EnqueueOfflineTo(&event_queue);
  // |event_queue| should not start an offline |event_3| because non-offline
  // events are running.
  //
  // State:
  // - inflight_events: {1 (normal), 2 (normal)}
  // - queue: [3 (offline)]
  EXPECT_FALSE(event_3.Started());

  MockEvent event_4;
  event_4.EnqueueOfflineTo(&event_queue);
  // State:
  // - inflight_events: {1 (normal), 2 (normal)}
  // - queue: [3 (offline), 4 (offline)]
  EXPECT_FALSE(event_3.Started());
  EXPECT_FALSE(event_4.Started());

  MockEvent event_5;
  event_5.EnqueueTo(&event_queue);
  // |event_queue| should not start a normal |event_5| because an offline event
  // is already enqueued.
  //
  // State:
  // - inflight_events: {1 (normal), 2 (normal)}
  // - queue: [3 (offline), 4 (offline), 5 (normal)]
  EXPECT_FALSE(event_3.Started());
  EXPECT_FALSE(event_4.Started());
  EXPECT_FALSE(event_5.Started());

  event_queue.EndEvent(event_1.event_id());
  // |event_1| is finished, but there ia still an inflight event, |event_2|.
  // Events in the queue are not processed.
  //
  // State:
  // - inflight_events: {2 (normal)}
  // - queue: [3 (offline), 4 (offline), 5 (normal)]
  EXPECT_FALSE(event_3.Started());
  EXPECT_FALSE(event_4.Started());
  EXPECT_FALSE(event_5.Started());

  event_queue.EndEvent(event_2.event_id());
  // All inflight events are finished. |event_queue| starts processing
  // events in the queue. As a result, |event_3| and |event_4| are started, but
  // |event_5| are not.
  //
  // State:
  // - inflight_events: {3 (offline), 4 (offline)}
  // - queue: [5 (normal)]
  EXPECT_TRUE(event_3.Started());
  EXPECT_TRUE(event_4.Started());
  EXPECT_FALSE(event_5.Started());

  event_queue.EndEvent(event_3.event_id());
  // State:
  // - inflight_events: {4 (offline)}
  // - queue: [5 (normal)]
  EXPECT_FALSE(event_5.Started());

  event_queue.EndEvent(event_4.event_id());
  // State:
  // - inflight_events: {5 (normal)}
  // - queue: []
  EXPECT_TRUE(event_5.Started());
}

TEST_F(ServiceWorkerEventQueueTest, IdleTimerWithOfflineEvents) {
  const base::TimeDelta kIdleInterval = base::TimeDelta::FromSeconds(
      mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds);

  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(
      base::DoNothing(), CreateReceiverWithCalledFlag(&is_idle), task_runner(),
      task_runner()->GetMockTickClock());
  event_queue.Start();

  MockEvent event1;
  event1.EnqueueTo(&event_queue);
  // State:
  // - inflight_events: {1 (normal)}
  // - queue: []
  EXPECT_TRUE(event1.Started());
  task_runner()->FastForwardBy(kIdleInterval);
  EXPECT_FALSE(is_idle);

  MockEvent event2;
  event2.EnqueueOfflineTo(&event_queue);
  task_runner()->FastForwardBy(kIdleInterval);
  // State:
  // - inflight_events: {1 (normal)}
  // - queue: [2 (offline)]
  EXPECT_FALSE(event2.Started());
  EXPECT_FALSE(is_idle);

  event_queue.EndEvent(event1.event_id());
  task_runner()->FastForwardBy(kIdleInterval);
  // State:
  // - inflight_events: {2 (offline)}
  // - queue: []
  EXPECT_TRUE(event2.Started());
  EXPECT_FALSE(is_idle);

  event_queue.EndEvent(event2.event_id());
  // State:
  // - inflight_events: {}
  // - queue: []
  EXPECT_FALSE(is_idle);
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired.
  EXPECT_TRUE(is_idle);
}

}  // namespace blink
