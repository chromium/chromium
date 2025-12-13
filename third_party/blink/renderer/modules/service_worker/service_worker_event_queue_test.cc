// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_event_queue.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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

  const std::optional<mojom::blink::ServiceWorkerEventStatus>& status() const {
    return status_;
  }

  bool Started() const { return started_; }

  void EnqueueTo(ServiceWorkerEventQueue* event_queue) {
    event_id_ = event_queue->NextEventId();
    event_queue->EnqueueNormal(
        *event_id_,
        blink::BindOnce(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        blink::BindOnce(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        std::nullopt);
  }

  void EnqueuePendingTo(ServiceWorkerEventQueue* event_queue) {
    event_id_ = event_queue->NextEventId();
    event_queue->EnqueuePending(
        *event_id_,
        blink::BindOnce(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        blink::BindOnce(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        std::nullopt);
  }

  void EnqueueWithCustomTimeoutTo(ServiceWorkerEventQueue* event_queue,
                                  base::TimeDelta custom_timeout) {
    event_id_ = event_queue->NextEventId();
    event_queue->EnqueueNormal(
        *event_id_,
        blink::BindOnce(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        blink::BindOnce(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        custom_timeout);
  }

  void EnqueuePendingWithCustomTimeoutTo(ServiceWorkerEventQueue* event_queue,
                                         base::TimeDelta custom_timeout) {
    event_id_ = event_queue->NextEventId();
    event_queue->EnqueuePending(
        *event_id_,
        blink::BindOnce(&MockEvent::Start, weak_factory_.GetWeakPtr()),
        blink::BindOnce(&MockEvent::Abort, weak_factory_.GetWeakPtr()),
        custom_timeout);
  }

  void EnqueuePendingDispatchingEventTo(ServiceWorkerEventQueue* event_queue,
                                        String tag,
                                        Vector<String>* out_tags) {
    event_id_ = event_queue->NextEventId();
    event_queue->EnqueuePending(
        *event_id_,
        BindOnce(
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
            Unretained(event_queue), Unretained(this), std::move(tag),
            Unretained(out_tags)),
        base::DoNothing(), std::nullopt);
  }

 private:
  void Start(int event_id) {
    EXPECT_FALSE(Started());
    EXPECT_EQ(event_id_, event_id);
    started_ = true;
  }

  void Abort(int event_id, mojom::blink::ServiceWorkerEventStatus status) {
    EXPECT_EQ(event_id_, event_id);
    EXPECT_FALSE(status_.has_value());
    status_ = status;
  }

  std::optional<int> event_id_;
  std::optional<mojom::blink::ServiceWorkerEventStatus> status_;
  bool started_ = false;
  base::WeakPtrFactory<MockEvent> weak_factory_{this};
};

base::RepeatingClosure CreateReceiverWithCalledFlag(bool* out_is_called) {
  return BindRepeating([](bool* out_is_called) { *out_is_called = true; },
                       Unretained(out_is_called));
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
  test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext>
      task_runner_context_;
};

TEST_F(ServiceWorkerEventQueueTest, IdleTimer) {
  const base::TimeDelta kIdleInterval =
      base::Seconds(mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds);

  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
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
  const base::TimeDelta kIdleInterval =
      base::Seconds(mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds);

  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  MockEvent event;
  event.EnqueueTo(&event_queue);
  event_queue.Start();
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);
}

TEST_F(ServiceWorkerEventQueueTest, EventTimer) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();

  MockEvent event1, event2;
  event1.EnqueueTo(&event_queue);
  event2.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::Seconds(1));

  EXPECT_FALSE(event1.status().has_value());
  EXPECT_FALSE(event2.status().has_value());
  event_queue.EndEvent(event1.event_id());
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kEventTimeout +
                               base::Seconds(1));

  EXPECT_FALSE(event1.status().has_value());
  EXPECT_TRUE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event2.status().value());
}

TEST_F(ServiceWorkerEventQueueTest, CustomTimeouts) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  MockEvent event1, event2;
  event1.EnqueueWithCustomTimeoutTo(
      &event_queue,
      ServiceWorkerEventQueue::kUpdateInterval - base::Seconds(1));
  event2.EnqueueWithCustomTimeoutTo(
      &event_queue,
      ServiceWorkerEventQueue::kUpdateInterval * 2 - base::Seconds(1));
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::Seconds(1));

  EXPECT_TRUE(event1.status().has_value());
  EXPECT_FALSE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event1.status().value());
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::Seconds(1));

  EXPECT_TRUE(event1.status().has_value());
  EXPECT_TRUE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event2.status().value());
}

TEST_F(ServiceWorkerEventQueueTest, BecomeIdleAfterAbort) {
  bool is_idle = false;
  ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                      task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();

  MockEvent event;
  event.EnqueueTo(&event_queue);
  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kEventTimeout +
                               ServiceWorkerEventQueue::kUpdateInterval +
                               base::Seconds(1));

  // |event| should have been aborted, and at the same time, the idle timeout
  // should also be fired since there has been an aborted event.
  EXPECT_TRUE(event.status().has_value());
  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerEventQueueTest, AbortAllOnDestruction) {
  MockEvent event1, event2;
  {
    ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();

    event1.EnqueueTo(&event_queue);
    event2.EnqueueTo(&event_queue);

    task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                                 base::Seconds(1));

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
  ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  task_runner()->FastForwardBy(
      base::Seconds(mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds));
  EXPECT_TRUE(event_queue.did_idle_timeout());

  MockEvent pending_event;
  pending_event.EnqueuePendingTo(&event_queue);
  EXPECT_FALSE(pending_event.Started());

  // Start a new event. EnqueueEvent() should run the pending tasks.
  MockEvent event;
  event.EnqueueTo(&event_queue);
  EXPECT_FALSE(event_queue.did_idle_timeout());
  EXPECT_TRUE(pending_event.Started());
  EXPECT_TRUE(event.Started());
}

// Test that pending tasks are run when StartEvent() is called while there the
// idle event_queue.delay is zero. Regression test for https://crbug.com/878608.
TEST_F(ServiceWorkerEventQueueTest, RunPendingTasksWithZeroIdleTimerDelay) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  event_queue.SetIdleDelay(base::Seconds(0));
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(event_queue.did_idle_timeout());

  MockEvent event1, event2;
  Vector<String> handled_tasks;
  event1.EnqueuePendingDispatchingEventTo(&event_queue, "1", &handled_tasks);
  event2.EnqueuePendingDispatchingEventTo(&event_queue, "2", &handled_tasks);
  EXPECT_TRUE(handled_tasks.empty());

  // Start a new event. EnqueueEvent() should run the pending tasks.
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
    ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                        task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();
    EXPECT_FALSE(is_idle);

    event_queue.SetIdleDelay(base::Seconds(0));
    task_runner()->RunUntilIdle();
    // |idle_callback| should be fired since there is no event.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                        task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();
    MockEvent event;
    event.EnqueueTo(&event_queue);
    event_queue.SetIdleDelay(base::Seconds(0));
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
    ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                        task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();
    MockEvent event1, event2;
    event1.EnqueueTo(&event_queue);
    event2.EnqueueTo(&event_queue);
    event_queue.SetIdleDelay(base::Seconds(0));
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
    ServiceWorkerEventQueue event_queue(CreateReceiverWithCalledFlag(&is_idle),
                                        task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();
    std::unique_ptr<StayAwakeToken> token_1 =
        event_queue.CreateStayAwakeToken();
    std::unique_ptr<StayAwakeToken> token_2 =
        event_queue.CreateStayAwakeToken();
    event_queue.SetIdleDelay(base::Seconds(0));
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

// Inflight events must be aborted when event queue is destructed.
TEST_F(ServiceWorkerEventQueueTest, AbortInFlightEventOnDestruction) {
  MockEvent event;
  {
    ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();
    event.EnqueueTo(&event_queue);

    EXPECT_TRUE(event.Started());
    EXPECT_FALSE(event.status().has_value());
  }

  EXPECT_TRUE(event.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::ABORTED,
            event.status().value());
}

// Queued events must be aborted when event queue is destructed.
TEST_F(ServiceWorkerEventQueueTest, AbortQueuedEventOnDestruction) {
  MockEvent event;
  {
    ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                        task_runner()->GetMockTickClock());
    event_queue.Start();
    task_runner()->FastForwardBy(
        base::Seconds(mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds));
    ASSERT_TRUE(event_queue.did_idle_timeout());
    event.EnqueuePendingTo(&event_queue);

    EXPECT_FALSE(event.Started());
    EXPECT_FALSE(event.status().has_value());
  }

  EXPECT_TRUE(event.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::ABORTED,
            event.status().value());
  EXPECT_FALSE(event.Started());
}

// Timer for timeout of each event starts when the event is queued.
TEST_F(ServiceWorkerEventQueueTest, TimeoutNotStartedEvent) {
  ServiceWorkerEventQueue event_queue(base::DoNothing(), task_runner(),
                                      task_runner()->GetMockTickClock());
  event_queue.Start();
  task_runner()->FastForwardBy(
      base::Seconds(mojom::blink::kServiceWorkerDefaultIdleDelayInSeconds));
  ASSERT_TRUE(event_queue.did_idle_timeout());

  MockEvent event1, event2;
  event1.EnqueuePendingWithCustomTimeoutTo(
      &event_queue,
      ServiceWorkerEventQueue::kUpdateInterval * 2 - base::Seconds(1));

  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval);

  event2.EnqueueWithCustomTimeoutTo(
      &event_queue,
      ServiceWorkerEventQueue::kUpdateInterval - base::Seconds(1));

  EXPECT_TRUE(event1.Started());
  EXPECT_TRUE(event2.Started());

  task_runner()->FastForwardBy(ServiceWorkerEventQueue::kUpdateInterval +
                               base::Seconds(1));

  EXPECT_TRUE(event1.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event1.status().value());
  EXPECT_TRUE(event2.status().has_value());
  EXPECT_EQ(mojom::blink::ServiceWorkerEventStatus::TIMEOUT,
            event2.status().value());
  EXPECT_FALSE(event_queue.HasEvent(event1.event_id()));
  EXPECT_FALSE(event_queue.HasEventInQueue(event1.event_id()));
  EXPECT_FALSE(event_queue.HasEvent(event2.event_id()));
  EXPECT_FALSE(event_queue.HasEventInQueue(event2.event_id()));
}

}  // namespace blink
