// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ScriptedAnimationControllerTest : public testing::Test {
 protected:
  void SetUp() override;

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
    return WTF::BindRepeating(&TaskOrderObserver::RunTask,
                              WTF::Unretained(this), id);
  }
  const Vector<int>& Order() const { return order_; }

 private:
  void RunTask(int id) { order_.push_back(id); }
  Vector<int> order_;
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
  Controller().EnqueueTask(WTF::BindOnce(&EnqueueTask,
                                         WrapPersistent(&Controller()),
                                         WTF::Unretained(&observer), 2));
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
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)));
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
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)));
  EXPECT_TRUE(Controller().HasFrameCallback());

  Controller().CancelFrameCallback(1);
  EXPECT_FALSE(Controller().HasFrameCallback());

  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(1)));
  Controller().RegisterFrameCallback(
      MakeGarbageCollected<RunTaskCallback>(observer.CreateTask(2)));
  EXPECT_TRUE(Controller().HasFrameCallback());

  Controller().CancelFrameCallback(1);
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
      MakeGarbageCollected<RunTaskCallback>(WTF::BindRepeating(
          [](ScriptedAnimationController* controller, bool* ran_callback) {
            EXPECT_TRUE(
                controller->GetExecutionContext()->IsInRequestAnimationFrame());
            *ran_callback = true;
          },
          WrapPersistent(&Controller()), WTF::Unretained(&ran_callback))));

  PageAnimator::ServiceScriptedAnimations(base::TimeTicks(),
                                          {{Controller(), false}});
  EXPECT_TRUE(ran_callback);

  EXPECT_FALSE(Controller().GetExecutionContext()->IsInRequestAnimationFrame());
}

}  // namespace blink
