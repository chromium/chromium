// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/common/widget_messages.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

using ::testing::_;

namespace content {
namespace {

class MockLockTarget : public MouseLockDispatcher::LockTarget {
 public:
   MOCK_METHOD1(OnLockMouseACK, void(bool));
   MOCK_METHOD0(OnMouseLockLost, void());
   MOCK_METHOD1(HandleMouseLockedInputEvent,
                bool(const blink::WebMouseEvent&));
};

// MouseLockDispatcher is a RenderViewObserver, and we test it by creating a
// fixture containing a RenderViewImpl view() and interacting to that interface.
class MouseLockDispatcherTest : public RenderViewTest {
 public:
  void SetUp() override {
    RenderViewTest::SetUp();
    route_id_ = view()->GetWidget()->routing_id();
    target_ = new MockLockTarget();
    alternate_target_ = new MockLockTarget();
  }

  void TearDown() override {
    RenderViewTest::TearDown();
    delete target_;
    delete alternate_target_;
  }

 protected:
  RenderViewImpl* view() { return static_cast<RenderViewImpl*>(view_); }
  RenderWidget* widget() { return view()->GetWidget(); }
  MouseLockDispatcher* dispatcher() {
    return widget()->mouse_lock_dispatcher();
  }
  int route_id_;
  MockLockTarget* target_;
  MockLockTarget* alternate_target_;
};

}  // namespace

// Test simple use of RenderViewImpl interface for pointer lock.
TEST_F(MouseLockDispatcherTest, BasicWebWidget) {
  // Start unlocked.
  EXPECT_FALSE(widget()->IsPointerLocked());

  // Lock.
  EXPECT_TRUE(
      widget()->RequestPointerLock(view()->GetMainRenderFrame()->GetWebFrame(),
                                   false /* unadjusted_movement */));
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, true));
  EXPECT_TRUE(widget()->IsPointerLocked());

  // Unlock.
  widget()->RequestPointerUnlock();
  widget()->OnMessageReceived(WidgetMsg_MouseLockLost(route_id_));
  EXPECT_FALSE(widget()->IsPointerLocked());

  // Attempt a lock, and have it fail.
  EXPECT_TRUE(
      widget()->RequestPointerLock(view()->GetMainRenderFrame()->GetWebFrame(),
                                   false /* unadjusted_movement */));
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, false));
  EXPECT_FALSE(widget()->IsPointerLocked());
}

// Test simple use of MouseLockDispatcher with a mock LockTarget.
TEST_F(MouseLockDispatcherTest, BasicMockLockTarget) {
  ::testing::InSequence expect_calls_in_sequence;
  EXPECT_CALL(*target_, OnLockMouseACK(true));
  EXPECT_CALL(*target_, HandleMouseLockedInputEvent(_));
  EXPECT_CALL(*target_, OnMouseLockLost());
  EXPECT_CALL(*target_, OnLockMouseACK(false));

  // Start unlocked.
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(nullptr));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(target_));

  // Lock.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, true));
  EXPECT_TRUE(dispatcher()->IsMouseLockedTo(target_));

  // Receive mouse event.
  dispatcher()->WillHandleMouseEvent(blink::WebMouseEvent());

  // Unlock.
  dispatcher()->UnlockMouse(target_);
  widget()->OnMessageReceived(WidgetMsg_MouseLockLost(route_id_));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(target_));

  // Attempt a lock, and have it fail.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, false));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(target_));
}

// Test deleting a target while it is in use by MouseLockDispatcher.
TEST_F(MouseLockDispatcherTest, DeleteAndUnlock) {
  ::testing::InSequence expect_calls_in_sequence;
  EXPECT_CALL(*target_, OnLockMouseACK(true));
  EXPECT_CALL(*target_, HandleMouseLockedInputEvent(_)).Times(0);
  EXPECT_CALL(*target_, OnMouseLockLost()).Times(0);

  // Lock.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, true));
  EXPECT_TRUE(dispatcher()->IsMouseLockedTo(target_));

  // Unlock, with a deleted target.
  // Don't receive mouse events or lock lost.
  dispatcher()->OnLockTargetDestroyed(target_);
  delete target_;
  target_ = nullptr;
  dispatcher()->WillHandleMouseEvent(blink::WebMouseEvent());
  widget()->OnMessageReceived(WidgetMsg_MouseLockLost(route_id_));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(target_));
}

// Test deleting a target that is pending a lock request response.
TEST_F(MouseLockDispatcherTest, DeleteWithPendingLockSuccess) {
  ::testing::InSequence expect_calls_in_sequence;
  EXPECT_CALL(*target_, OnLockMouseACK(true)).Times(0);
  EXPECT_CALL(*target_, OnMouseLockLost()).Times(0);

  // Lock request.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));

  // Before receiving response delete the target.
  dispatcher()->OnLockTargetDestroyed(target_);
  delete target_;
  target_ = nullptr;

  // Lock response.
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, true));
}

// Test deleting a target that is pending a lock request failure response.
TEST_F(MouseLockDispatcherTest, DeleteWithPendingLockFail) {
  ::testing::InSequence expect_calls_in_sequence;
  EXPECT_CALL(*target_, OnLockMouseACK(true)).Times(0);
  EXPECT_CALL(*target_, OnMouseLockLost()).Times(0);

  // Lock request.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));

  // Before receiving response delete the target.
  dispatcher()->OnLockTargetDestroyed(target_);
  delete target_;
  target_ = nullptr;

  // Lock response.
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, false));
}

// Test not receiving mouse events when a target is not locked.
TEST_F(MouseLockDispatcherTest, MouseEventsNotReceived) {
  ::testing::InSequence expect_calls_in_sequence;
  EXPECT_CALL(*target_, HandleMouseLockedInputEvent(_)).Times(0);
  EXPECT_CALL(*target_, OnLockMouseACK(true));
  EXPECT_CALL(*target_, HandleMouseLockedInputEvent(_));
  EXPECT_CALL(*target_, OnMouseLockLost());
  EXPECT_CALL(*target_, HandleMouseLockedInputEvent(_)).Times(0);

  // (Don't) receive mouse event.
  dispatcher()->WillHandleMouseEvent(blink::WebMouseEvent());

  // Lock.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, true));
  EXPECT_TRUE(dispatcher()->IsMouseLockedTo(target_));

  // Receive mouse event.
  dispatcher()->WillHandleMouseEvent(blink::WebMouseEvent());

  // Unlock.
  dispatcher()->UnlockMouse(target_);
  widget()->OnMessageReceived(WidgetMsg_MouseLockLost(route_id_));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(target_));

  // (Don't) receive mouse event.
  dispatcher()->WillHandleMouseEvent(blink::WebMouseEvent());
}

// Test multiple targets
TEST_F(MouseLockDispatcherTest, MultipleTargets) {
  ::testing::InSequence expect_calls_in_sequence;
  EXPECT_CALL(*target_, OnLockMouseACK(true));
  EXPECT_CALL(*target_, HandleMouseLockedInputEvent(_));
  EXPECT_CALL(*alternate_target_, HandleMouseLockedInputEvent(_)).Times(0);
  EXPECT_CALL(*target_, OnMouseLockLost()).Times(0);
  EXPECT_CALL(*alternate_target_, OnMouseLockLost()).Times(0);
  EXPECT_CALL(*target_, OnMouseLockLost());

  // Lock request for target.
  EXPECT_TRUE(dispatcher()->LockMouse(
      target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));

  // Fail attempt to lock alternate.
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(alternate_target_));
  EXPECT_FALSE(dispatcher()->LockMouse(
      alternate_target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));

  // Lock completion for target.
  widget()->OnMessageReceived(WidgetMsg_LockMouse_ACK(route_id_, true));
  EXPECT_TRUE(dispatcher()->IsMouseLockedTo(target_));

  // Fail attempt to lock alternate.
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(alternate_target_));
  EXPECT_FALSE(dispatcher()->LockMouse(
      alternate_target_, view()->GetMainRenderFrame()->GetWebFrame(),
      false /* unadjusted_movement */));

  // Receive mouse event to only one target.
  dispatcher()->WillHandleMouseEvent(blink::WebMouseEvent());

  // Unlock alternate target has no effect.
  dispatcher()->UnlockMouse(alternate_target_);
  EXPECT_TRUE(dispatcher()->IsMouseLockedTo(target_));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(alternate_target_));

  // Though the call to UnlockMouse should not unlock any target, we will
  // cause an unlock (as if e.g. user escaped mouse lock) and verify the
  // correct target is unlocked.
  widget()->OnMessageReceived(WidgetMsg_MouseLockLost(route_id_));
  EXPECT_FALSE(dispatcher()->IsMouseLockedTo(target_));
}

}  // namespace content
