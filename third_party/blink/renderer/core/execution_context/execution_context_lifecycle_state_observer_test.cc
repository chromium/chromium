/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using testing::AnyNumber;

namespace blink {

class MockExecutionContextLifecycleStateObserver final
    : public GarbageCollected<MockExecutionContextLifecycleStateObserver>,
      public ExecutionContextLifecycleStateObserver {
 public:
  explicit MockExecutionContextLifecycleStateObserver(ExecutionContext* context)
      : ExecutionContextLifecycleStateObserver(context) {}

  void Trace(Visitor* visitor) const override {
    ExecutionContextLifecycleStateObserver::Trace(visitor);
  }

  MOCK_METHOD1(ContextLifecycleStateChanged, void(mojom::FrameLifecycleState));
  MOCK_METHOD0(ContextDestroyed, void());
};

class ExecutionContextLifecycleStateObserverTest : public testing::Test {
 protected:
  ExecutionContextLifecycleStateObserverTest();

  LocalDOMWindow* SrcWindow() const {
    return src_page_holder_->GetFrame().DomWindow();
  }
  LocalDOMWindow* DestWindow() const {
    return dest_page_holder_->GetFrame().DomWindow();
  }

  void ClearDestPage() { dest_page_holder_.reset(); }
  MockExecutionContextLifecycleStateObserver& Observer() { return *observer_; }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> src_page_holder_;
  std::unique_ptr<DummyPageHolder> dest_page_holder_;
  Persistent<MockExecutionContextLifecycleStateObserver> observer_;
};

ExecutionContextLifecycleStateObserverTest::
    ExecutionContextLifecycleStateObserverTest()
    : src_page_holder_(std::make_unique<DummyPageHolder>(gfx::Size(800, 600))),
      dest_page_holder_(std::make_unique<DummyPageHolder>(gfx::Size(800, 600))),
      observer_(
          MakeGarbageCollected<MockExecutionContextLifecycleStateObserver>(
              src_page_holder_->GetFrame().DomWindow())) {
  observer_->UpdateStateIfNeeded();
}

TEST_F(ExecutionContextLifecycleStateObserverTest, NewContextObserved) {
  unsigned initial_src_count =
      SrcWindow()->ContextLifecycleStateObserverCountForTesting();
  unsigned initial_dest_count =
      DestWindow()->ContextLifecycleStateObserverCountForTesting();

  EXPECT_CALL(Observer(), ContextLifecycleStateChanged(
                              mojom::FrameLifecycleState::kRunning));
  EXPECT_CALL(Observer(), ContextDestroyed()).Times(AnyNumber());
  Observer().SetExecutionContext(DestWindow());

  EXPECT_EQ(initial_src_count - 1,
            SrcWindow()->ContextLifecycleStateObserverCountForTesting());
  EXPECT_EQ(initial_dest_count + 1,
            DestWindow()->ContextLifecycleStateObserverCountForTesting());
}

TEST_F(ExecutionContextLifecycleStateObserverTest, MoveToActiveContext) {
  EXPECT_CALL(Observer(), ContextLifecycleStateChanged(
                              mojom::FrameLifecycleState::kRunning));
  EXPECT_CALL(Observer(), ContextDestroyed()).Times(AnyNumber());
  Observer().SetExecutionContext(DestWindow());
}

TEST_F(ExecutionContextLifecycleStateObserverTest, MoveToSuspendedContext) {
  DestWindow()->SetLifecycleState(mojom::FrameLifecycleState::kFrozen);

  EXPECT_CALL(Observer(), ContextLifecycleStateChanged(
                              mojom::FrameLifecycleState::kFrozen));
  EXPECT_CALL(Observer(), ContextDestroyed()).Times(AnyNumber());
  Observer().SetExecutionContext(DestWindow());
}

TEST_F(ExecutionContextLifecycleStateObserverTest, MoveToStoppedContext) {
  Persistent<LocalDOMWindow> window = DestWindow();
  ClearDestPage();
  EXPECT_CALL(Observer(), ContextDestroyed());
  Observer().SetExecutionContext(window.Get());
}

}  // namespace blink
