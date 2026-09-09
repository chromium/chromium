// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"

#include <cmath>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ScriptedAnimationControllerTest : public testing::Test {
 protected:
  void SetUp() override;

  Page& GetPage() const { return dummy_page_holder_->GetPage(); }
  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }
  ScriptedAnimationController& Controller() { return *controller_; }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<ScriptedAnimationController> controller_;
};

void ScriptedAnimationControllerTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));

  // Note: The document doesn't know about this ScriptedAnimationController
  // instance.
  controller_ =
      WrapPersistent(MakeGarbageCollected<ScriptedAnimationController>(
          dummy_page_holder_->GetFrame().DomWindow()));
}

namespace {

class TaskOrderObserver {
  STACK_ALLOCATED();

 public:
  base::RepeatingClosure CreateTask(int id) {
    return BindRepeating(&TaskOrderObserver::RunTask,
                         blink::subtle::UnretainedException(this), id);
  }
  const Vector<int>& Order() const { return order_; }

 private:
  void RunTask(int id) { order_.push_back(id); }
  Vector<int> order_;
};

class RecordTimestampCallback final : public FrameCallback {
 public:
  explicit RecordTimestampCallback(double* recorded_time)
      : recorded_time_(recorded_time) {}
  void Invoke(double timestamp) override { *recorded_time_ = timestamp; }

 private:
  raw_ptr<double> recorded_time_;
};

}  // anonymous namespace

TEST_F(ScriptedAnimationControllerTest, EnqueueOneTask) {
  TaskOrderObserver observer;

  Controller().EnqueueTask(observer.CreateTask(1));
  EXPECT_EQ(0u, observer.Order().size());

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(1u, observer.Order().size());
  EXPECT_EQ(1, observer.Order()[0]);
}

TEST_F(ScriptedAnimationControllerTest, EnqueueTwoTasks) {
  TaskOrderObserver observer;

  Controller().EnqueueTask(observer.CreateTask(1));
  Controller().EnqueueTask(observer.CreateTask(2));
  EXPECT_EQ(0u, observer.Order().size());

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(2u, observer.Order().size());
  EXPECT_EQ(1, observer.Order()[0]);
  EXPECT_EQ(2, observer.Order()[1]);
}

namespace {

void EnqueueTask(ScriptedAnimationController* controller,
                 TaskOrderObserver* observer,
                 int id) {
  controller->EnqueueTask(observer->CreateTask(id));
}

}  // anonymous namespace

// A task enqueued while running tasks should not be run immediately after, but
// the next time tasks are run.
TEST_F(ScriptedAnimationControllerTest, EnqueueWithinTask) {
  TaskOrderObserver observer;

  Controller().EnqueueTask(observer.CreateTask(1));
  Controller().EnqueueTask(
      BindOnce(&EnqueueTask, WrapPersistent(&Controller()),
               blink::subtle::UnretainedException(&observer), 2));
  Controller().EnqueueTask(observer.CreateTask(3));
  EXPECT_EQ(0u, observer.Order().size());

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(2u, observer.Order().size());
  EXPECT_EQ(1, observer.Order()[0]);
  EXPECT_EQ(3, observer.Order()[1]);

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(3u, observer.Order().size());
  EXPECT_EQ(1, observer.Order()[0]);
  EXPECT_EQ(3, observer.Order()[1]);
  EXPECT_EQ(2, observer.Order()[2]);
}

namespace {

class RunTaskEventListener final : public NativeEventListener {
 public:
  RunTaskEventListener(base::RepeatingClosure task) : task_(std::move(task)) {}
  void Invoke(ExecutionContext*, Event*) override { task_.Run(); }

 private:
  base::RepeatingClosure task_;
};

}  // anonymous namespace

// Tasks should be run after events are dispatched, even if they were enqueued
// first.
TEST_F(ScriptedAnimationControllerTest, EnqueueTaskAndEvent) {
  TaskOrderObserver observer;

  Controller().EnqueueTask(observer.CreateTask(1));
  GetDocument().addEventListener(
      AtomicString("test"),
      MakeGarbageCollected<RunTaskEventListener>(observer.CreateTask(2)));
  Event* event = Event::Create(AtomicString("test"));
  event->SetTarget(&GetDocument());
  Controller().EnqueueEvent(event);
  EXPECT_EQ(0u, observer.Order().size());

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(2u, observer.Order().size());
  EXPECT_EQ(2, observer.Order()[0]);
  EXPECT_EQ(1, observer.Order()[1]);
}

namespace {

class RunTaskCallback final : public FrameCallback {
 public:
  RunTaskCallback(base::RepeatingClosure task) : task_(std::move(task)) {}
  void Invoke(double) override { task_.Run(); }

 private:
  base::RepeatingClosure task_;
};

}  // anonymous namespace

// Animation frame callbacks should be run after tasks, even if they were
// enqueued first.
TEST_F(ScriptedAnimationControllerTest, RegisterCallbackAndEnqueueTask) {
  TaskOrderObserver observer;

  Event* event = Event::Create(AtomicString("test"));
  event->SetTarget(&GetDocument());

  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)),
      FrameCallbackType::kWebExposed);
  Controller().EnqueueTask(observer.CreateTask(2));
  EXPECT_EQ(0u, observer.Order().size());

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(2u, observer.Order().size());
  EXPECT_EQ(2, observer.Order()[0]);
  EXPECT_EQ(1, observer.Order()[1]);
}

TEST_F(ScriptedAnimationControllerTest, TestHasCallback) {
  TaskOrderObserver observer;

  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)),
      FrameCallbackType::kWebExposed);
  EXPECT_TRUE(Controller().HasFrameCallback());

  Controller().CancelFrameCallback(1, FrameCallbackType::kWebExposed);
  EXPECT_FALSE(Controller().HasFrameCallback());

  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)),
      FrameCallbackType::kWebExposed);
  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(2)),
      FrameCallbackType::kWebExposed);
  EXPECT_TRUE(Controller().HasFrameCallback());

  Controller().CancelFrameCallback(1, FrameCallbackType::kWebExposed);
  EXPECT_TRUE(Controller().HasFrameCallback());

  // Servicing the scripted animations should call the remaining callback and
  // clear it.
  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_FALSE(Controller().HasFrameCallback());
}

TEST_F(ScriptedAnimationControllerTest, TestIsInRequestAnimationFrame) {
  EXPECT_FALSE(Controller().GetExecutionContext()->IsInRequestAnimationFrame());

  bool ran_callback = false;
  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(BindRepeating(
          [](ScriptedAnimationController* controller, bool* ran_callback) {
            EXPECT_TRUE(
                controller->GetExecutionContext()->IsInRequestAnimationFrame());
            *ran_callback = true;
          },
          WrapPersistent(&Controller()),
          blink::subtle::UnretainedException(&ran_callback))),
      FrameCallbackType::kWebExposed);

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_TRUE(ran_callback);

  EXPECT_FALSE(Controller().GetExecutionContext()->IsInRequestAnimationFrame());
}

TEST_F(ScriptedAnimationControllerTest, TestInternalCallbackIsolation) {
  TaskOrderObserver observer;

  // Web-exposed callback registration gets ID 1 in web pool.
  int web_id = Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)),
      FrameCallbackType::kWebExposed);
  EXPECT_EQ(1, web_id);

  // Internal callback registration gets independent ID 1 in internal pool.
  int internal_id = Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(2)),
      FrameCallbackType::kInternal);
  EXPECT_EQ(1, internal_id);

  // Canceling internal callback with FrameCallbackType::kInternal should cancel
  // internal without affecting web callback.
  Controller().CancelFrameCallback(internal_id, FrameCallbackType::kInternal);

  // Servicing scripted animations should run only the web callback.
  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(1u, observer.Order().size());
  EXPECT_EQ(1, observer.Order()[0]);
}

TEST_F(ScriptedAnimationControllerTest,
       TestCancelWebCallbackDoesNotCancelInternalCallback) {
  TaskOrderObserver observer;

  int web_id = Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)),
      FrameCallbackType::kWebExposed);
  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(2)),
      FrameCallbackType::kInternal);

  // CancelFrameCallback with kWebExposed cancels web callback without
  // affecting internal callback.
  Controller().CancelFrameCallback(web_id, FrameCallbackType::kWebExposed);

  // Servicing scripted animations should run only the internal callback.
  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_EQ(1u, observer.Order().size());
  EXPECT_EQ(2, observer.Order()[0]);
}

TEST_F(ScriptedAnimationControllerTest, CoarsenedFrameTimestamps) {
  ScriptedAnimationController& document_controller =
      GetDocument().GetScriptedAnimationController();

  base::TimeTicks zero_time = GetDocument().Timeline().CalculateZeroTime();
  double reference_wall_time_ms = GetDocument()
                                      .Loader()
                                      ->GetTiming()
                                      .MonotonicTimeToPseudoWallTime(zero_time)
                                      .InMillisecondsF();

  double previous_standard_time = -1.0;
  double previous_legacy_time = -1.0;

  for (int frame = 0; frame < 5; ++frame) {
    double standard_time = -1.0;
    double legacy_time = -1.0;

    auto* standard_cb =
        MakeGarbageCollected<RecordTimestampCallback>(&standard_time);
    standard_cb->SetUseLegacyTimeBase(false);
    document_controller.RegisterFrameCallback(standard_cb,
                                              FrameCallbackType::kWebExposed);

    auto* legacy_cb =
        MakeGarbageCollected<RecordTimestampCallback>(&legacy_time);
    legacy_cb->SetUseLegacyTimeBase(true);
    document_controller.RegisterFrameCallback(legacy_cb,
                                              FrameCallbackType::kWebExposed);

    // Frame 0 tests zero time (document_timeline_time_ms == 0). Subsequent
    // frames provide non-aligned microsecond intervals: 16ms simulates a ~60Hz
    // frame interval, while (13 * frame + 7) microseconds adds prime-stepping
    // sub-100us jitter (7us, 20us, 33us, 46us, 59us) that is never a multiple
    // of kCoarseResolutionMicroseconds (100us), ensuring TimeClamper actively
    // coarsens every frame timestamp.
    base::TimeTicks frame_time = frame == 0
                                     ? zero_time
                                     : zero_time + base::Seconds(1) +
                                           base::Milliseconds(16 * frame) +
                                           base::Microseconds(13 * frame + 7);
    GetPage().Animator().ServiceScriptedAnimations(frame_time);

    EXPECT_GE(standard_time, 0.0);
    EXPECT_GE(legacy_time, reference_wall_time_ms);
    EXPECT_NEAR(legacy_time, reference_wall_time_ms + standard_time, 0.001);

    if (frame > 0) {
      double standard_delta = standard_time - previous_standard_time;
      double legacy_delta = legacy_time - previous_legacy_time;

      EXPECT_NEAR(legacy_delta, standard_delta, 0.001);

      int64_t standard_delta_us =
          static_cast<int64_t>(std::round(standard_delta * 1000.0));
      int64_t legacy_delta_us =
          static_cast<int64_t>(std::round(legacy_delta * 1000.0));

      EXPECT_EQ(legacy_delta_us, standard_delta_us);
      EXPECT_EQ(standard_delta_us % TimeClamper::kCoarseResolutionMicroseconds,
                0);
      EXPECT_EQ(legacy_delta_us % TimeClamper::kCoarseResolutionMicroseconds,
                0);
    }

    previous_standard_time = standard_time;
    previous_legacy_time = legacy_time;
  }
}

}  // namespace blink
