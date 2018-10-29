// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_timeout_timer.h"

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

namespace {

class MockEvent {
 public:
  MockEvent() : weak_factory_(this) {}

  base::OnceCallback<void(int)> CreateAbortCallback() {
    EXPECT_FALSE(has_aborted_);
    return base::BindOnce(&MockEvent::Abort, weak_factory_.GetWeakPtr());
  }

  int event_id() const { return event_id_; }
  void set_event_id(int event_id) { event_id_ = event_id; }
  bool has_aborted() const { return has_aborted_; }

 private:
  void Abort(int event_id) {
    EXPECT_EQ(event_id_, event_id);
    has_aborted_ = true;
  }

  bool has_aborted_ = false;
  int event_id_ = 0;
  base::WeakPtrFactory<MockEvent> weak_factory_;
};

base::RepeatingClosure CreateReceiverWithCalledFlag(bool* out_is_called) {
  return base::BindRepeating([](bool* out_is_called) { *out_is_called = true; },
                             out_is_called);
}

base::OnceClosure CreateDispatchingEventTask(
    ServiceWorkerTimeoutTimer* timer,
    std::string tag,
    std::vector<std::string>* out_tags) {
  return base::BindOnce(
      [](ServiceWorkerTimeoutTimer* timer, std::string tag,
         std::vector<std::string>* out_tags) {
        // Event dispatched inside of pending task should run successfully.
        MockEvent event;
        const int event_id = timer->StartEvent(event.CreateAbortCallback());
        event.set_event_id(event_id);
        EXPECT_FALSE(timer->did_idle_timeout());
        EXPECT_FALSE(event.has_aborted());

        out_tags->emplace_back(std::move(tag));

        timer->EndEvent(event_id);
        EXPECT_FALSE(event.has_aborted());
      },
      timer, std::move(tag), out_tags);
}

}  // namespace

using StayAwakeToken = ServiceWorkerTimeoutTimer::StayAwakeToken;

class ServiceWorkerTimeoutTimerTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    message_loop_.SetTaskRunner(task_runner_);
  }

  void EnableServicification() {
    feature_list_.InitWithFeatures({network::features::kNetworkService}, {});
    ASSERT_TRUE(blink::ServiceWorkerUtils::IsServicificationEnabled());
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServiceWorkerTimeoutTimerTest, IdleTimer) {
  EnableServicification();

  const base::TimeDelta kIdleInterval =
      ServiceWorkerTimeoutTimer::kIdleDelay +
      ServiceWorkerTimeoutTimer::kUpdateInterval +
      base::TimeDelta::FromSeconds(1);

  base::RepeatingCallback<void(int)> do_nothing_callback =
      base::BindRepeating([](int) {});

  bool is_idle = false;
  ServiceWorkerTimeoutTimer timer(CreateReceiverWithCalledFlag(&is_idle),
                                  task_runner()->GetMockTickClock());
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired since there is no event.
  EXPECT_TRUE(is_idle);

  is_idle = false;
  int event_id_1 = timer.StartEvent(do_nothing_callback);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  int event_id_2 = timer.StartEvent(do_nothing_callback);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there are two inflight events.
  EXPECT_FALSE(is_idle);

  timer.EndEvent(event_id_2);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  timer.EndEvent(event_id_1);
  task_runner()->FastForwardBy(kIdleInterval);
  // |idle_callback| should be fired.
  EXPECT_TRUE(is_idle);

  is_idle = false;
  int event_id_3 = timer.StartEvent(do_nothing_callback);
  task_runner()->FastForwardBy(kIdleInterval);
  // Nothing happens since there is an inflight event.
  EXPECT_FALSE(is_idle);

  std::unique_ptr<StayAwakeToken> token = timer.CreateStayAwakeToken();
  timer.EndEvent(event_id_3);
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

TEST_F(ServiceWorkerTimeoutTimerTest, EventTimer) {
  EnableServicification();

  ServiceWorkerTimeoutTimer timer(base::DoNothing(),
                                  task_runner()->GetMockTickClock());
  MockEvent event1, event2;

  int event_id1 = timer.StartEvent(event1.CreateAbortCallback());
  int event_id2 = timer.StartEvent(event2.CreateAbortCallback());
  event1.set_event_id(event_id1);
  event2.set_event_id(event_id2);
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(event1.has_aborted());
  EXPECT_FALSE(event2.has_aborted());
  timer.EndEvent(event1.event_id());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kEventTimeout +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_FALSE(event1.has_aborted());
  EXPECT_TRUE(event2.has_aborted());
}

TEST_F(ServiceWorkerTimeoutTimerTest, CustomTimeouts) {
  EnableServicification();

  ServiceWorkerTimeoutTimer timer(base::DoNothing(),
                                  task_runner()->GetMockTickClock());
  MockEvent event1, event2;
  int event_id1 = timer.StartEventWithCustomTimeout(
      event1.CreateAbortCallback(), ServiceWorkerTimeoutTimer::kUpdateInterval -
                                        base::TimeDelta::FromSeconds(1));
  int event_id2 = timer.StartEventWithCustomTimeout(
      event2.CreateAbortCallback(),
      ServiceWorkerTimeoutTimer::kUpdateInterval * 2 -
          base::TimeDelta::FromSeconds(1));
  event1.set_event_id(event_id1);
  event2.set_event_id(event_id2);
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(event1.has_aborted());
  EXPECT_FALSE(event2.has_aborted());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(event1.has_aborted());
  EXPECT_TRUE(event2.has_aborted());
}

TEST_F(ServiceWorkerTimeoutTimerTest, BecomeIdleAfterAbort) {
  EnableServicification();

  bool is_idle = false;
  ServiceWorkerTimeoutTimer timer(CreateReceiverWithCalledFlag(&is_idle),
                                  task_runner()->GetMockTickClock());

  MockEvent event;
  int event_id = timer.StartEvent(event.CreateAbortCallback());
  event.set_event_id(event_id);
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kEventTimeout +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));

  // |event| should have been aborted, and at the same time, the idle timeout
  // should also be fired since there has been an aborted event.
  EXPECT_TRUE(event.has_aborted());
  EXPECT_TRUE(is_idle);
}

TEST_F(ServiceWorkerTimeoutTimerTest, AbortAllOnDestruction) {
  EnableServicification();

  MockEvent event1, event2;
  {
    ServiceWorkerTimeoutTimer timer(base::DoNothing(),
                                    task_runner()->GetMockTickClock());

    int event_id1 = timer.StartEvent(event1.CreateAbortCallback());
    int event_id2 = timer.StartEvent(event2.CreateAbortCallback());
    event1.set_event_id(event_id1);
    event2.set_event_id(event_id2);
    task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    EXPECT_FALSE(event1.has_aborted());
    EXPECT_FALSE(event2.has_aborted());
  }

  EXPECT_TRUE(event1.has_aborted());
  EXPECT_TRUE(event2.has_aborted());
}

TEST_F(ServiceWorkerTimeoutTimerTest, PushPendingTask) {
  EnableServicification();
  ServiceWorkerTimeoutTimer timer(base::DoNothing(),
                                  task_runner()->GetMockTickClock());
  task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                               ServiceWorkerTimeoutTimer::kUpdateInterval +
                               base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(timer.did_idle_timeout());

  bool did_task_run = false;
  timer.PushPendingTask(CreateReceiverWithCalledFlag(&did_task_run));

  // Start a new event. StartEvent() should run the pending tasks.
  MockEvent event;
  const int event_id = timer.StartEvent(event.CreateAbortCallback());
  event.set_event_id(event_id);
  EXPECT_FALSE(timer.did_idle_timeout());
  EXPECT_TRUE(did_task_run);
}

// Test that pending tasks are run when StartEvent() is called while there the
// idle timer delay is zero. Regression test for https://crbug.com/878608.
TEST_F(ServiceWorkerTimeoutTimerTest, RunPendingTasksWithZeroIdleTimerDelay) {
  EnableServicification();
  ServiceWorkerTimeoutTimer timer(base::DoNothing(),
                                  task_runner()->GetMockTickClock());
  timer.SetIdleTimerDelayToZero();
  EXPECT_TRUE(timer.did_idle_timeout());

  std::vector<std::string> handled_tasks;
  timer.PushPendingTask(
      CreateDispatchingEventTask(&timer, "1", &handled_tasks));
  timer.PushPendingTask(
      CreateDispatchingEventTask(&timer, "2", &handled_tasks));

  // Start a new event. StartEvent() should run the pending tasks.
  MockEvent event;
  const int event_id = timer.StartEvent(event.CreateAbortCallback());
  event.set_event_id(event_id);
  EXPECT_FALSE(timer.did_idle_timeout());
  ASSERT_EQ(2u, handled_tasks.size());
  EXPECT_EQ("1", handled_tasks[0]);
  EXPECT_EQ("2", handled_tasks[1]);
}

TEST_F(ServiceWorkerTimeoutTimerTest, SetIdleTimerDelayToZero) {
  EnableServicification();
  {
    bool is_idle = false;
    ServiceWorkerTimeoutTimer timer(CreateReceiverWithCalledFlag(&is_idle),
                                    task_runner()->GetMockTickClock());
    EXPECT_FALSE(is_idle);

    timer.SetIdleTimerDelayToZero();
    // |idle_callback| should be fired since there is no event.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerTimeoutTimer timer(CreateReceiverWithCalledFlag(&is_idle),
                                    task_runner()->GetMockTickClock());
    int event_id = timer.StartEvent(base::BindOnce([](int) {}));
    timer.SetIdleTimerDelayToZero();
    // Nothing happens since there is an inflight event.
    EXPECT_FALSE(is_idle);

    timer.EndEvent(event_id);
    // EndEvent() immediately triggers the idle callback.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerTimeoutTimer timer(CreateReceiverWithCalledFlag(&is_idle),
                                    task_runner()->GetMockTickClock());
    int event_id_1 = timer.StartEvent(base::BindOnce([](int) {}));
    int event_id_2 = timer.StartEvent(base::BindOnce([](int) {}));
    timer.SetIdleTimerDelayToZero();
    // Nothing happens since there are two inflight events.
    EXPECT_FALSE(is_idle);

    timer.EndEvent(event_id_1);
    // Nothing happens since there is an inflight event.
    EXPECT_FALSE(is_idle);

    timer.EndEvent(event_id_2);
    // EndEvent() immediately triggers the idle callback when no inflight events
    // exist.
    EXPECT_TRUE(is_idle);
  }

  {
    bool is_idle = false;
    ServiceWorkerTimeoutTimer timer(CreateReceiverWithCalledFlag(&is_idle),
                                    task_runner()->GetMockTickClock());
    std::unique_ptr<StayAwakeToken> token_1 = timer.CreateStayAwakeToken();
    std::unique_ptr<StayAwakeToken> token_2 = timer.CreateStayAwakeToken();
    timer.SetIdleTimerDelayToZero();
    // Nothing happens since there are two living tokens.
    EXPECT_FALSE(is_idle);

    token_1.reset();
    // Nothing happens since there is an living token.
    EXPECT_FALSE(is_idle);

    token_2.reset();
    // EndEvent() immediately triggers the idle callback when no tokens exist.
    EXPECT_TRUE(is_idle);
  }
}

TEST_F(ServiceWorkerTimeoutTimerTest, NonS13nServiceWorker) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  MockEvent event;
  {
    bool is_idle = false;
    ServiceWorkerTimeoutTimer timer(
        base::BindRepeating([](bool* out_is_idle) { *out_is_idle = true; },
                            &is_idle),
        task_runner()->GetMockTickClock());

    int event_id = timer.StartEvent(event.CreateAbortCallback());
    event.set_event_id(event_id);
    task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kEventTimeout +
                                 ServiceWorkerTimeoutTimer::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    // Timed out events  should *NOT* be aborted in non-S13nServiceWorker.
    EXPECT_FALSE(event.has_aborted());
    EXPECT_FALSE(is_idle);

    task_runner()->FastForwardBy(ServiceWorkerTimeoutTimer::kIdleDelay +
                                 ServiceWorkerTimeoutTimer::kUpdateInterval +
                                 base::TimeDelta::FromSeconds(1));

    // |idle_callback| should *NOT* be fired in non-S13nServiceWorker.
    EXPECT_FALSE(is_idle);
  }

  // Events should be aborted when the timer is destructed.
  EXPECT_TRUE(event.has_aborted());
}

}  // namespace content
