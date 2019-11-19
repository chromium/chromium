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

#include "third_party/blink/renderer/core/execution_context/context_lifecycle_state_observer.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class MockContextLifecycleStateObserver final
    : public GarbageCollected<MockContextLifecycleStateObserver>,
      public ContextLifecycleStateObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MockContextLifecycleStateObserver);

 public:
  explicit MockContextLifecycleStateObserver(ExecutionContext* context)
      : ContextLifecycleStateObserver(context) {}

  void Trace(blink::Visitor* visitor) override {
    ContextLifecycleStateObserver::Trace(visitor);
  }

  MOCK_METHOD1(ContextLifecycleStateChanged, void(mojom::FrameLifecycleState));
  MOCK_METHOD1(ContextDestroyed, void(ExecutionContext*));
};

class ContextLifecycleStateObserverTest : public testing::Test {
 protected:
  ContextLifecycleStateObserverTest();

  Document& SrcDocument() const { return src_page_holder_->GetDocument(); }
  Document& DestDocument() const { return dest_page_holder_->GetDocument(); }
  MockContextLifecycleStateObserver& Observer() { return *observer_; }

 private:
  std::unique_ptr<DummyPageHolder> src_page_holder_;
  std::unique_ptr<DummyPageHolder> dest_page_holder_;
  Persistent<MockContextLifecycleStateObserver> observer_;
};

ContextLifecycleStateObserverTest::ContextLifecycleStateObserverTest()
    : src_page_holder_(std::make_unique<DummyPageHolder>(IntSize(800, 600))),
      dest_page_holder_(std::make_unique<DummyPageHolder>(IntSize(800, 600))),
      observer_(MakeGarbageCollected<MockContextLifecycleStateObserver>(
          &src_page_holder_->GetDocument())) {
  observer_->UpdateStateIfNeeded();
}

TEST_F(ContextLifecycleStateObserverTest, NewContextObserved) {
  unsigned initial_src_count =
      SrcDocument().ContextLifecycleStateObserverCount();
  unsigned initial_dest_count =
      DestDocument().ContextLifecycleStateObserverCount();

  EXPECT_CALL(Observer(), ContextLifecycleStateChanged(
                              mojom::FrameLifecycleState::kRunning));
  Observer().DidMoveToNewExecutionContext(&DestDocument());

  EXPECT_EQ(initial_src_count - 1,
            SrcDocument().ContextLifecycleStateObserverCount());
  EXPECT_EQ(initial_dest_count + 1,
            DestDocument().ContextLifecycleStateObserverCount());
}

TEST_F(ContextLifecycleStateObserverTest, MoveToActiveDocument) {
  EXPECT_CALL(Observer(), ContextLifecycleStateChanged(
                              mojom::FrameLifecycleState::kRunning));
  Observer().DidMoveToNewExecutionContext(&DestDocument());
}

TEST_F(ContextLifecycleStateObserverTest, MoveToSuspendedDocument) {
  DestDocument().SetLifecycleState(mojom::FrameLifecycleState::kFrozen);

  EXPECT_CALL(Observer(), ContextLifecycleStateChanged(
                              mojom::FrameLifecycleState::kFrozen));
  Observer().DidMoveToNewExecutionContext(&DestDocument());
}

TEST_F(ContextLifecycleStateObserverTest, MoveToStoppedDocument) {
  DestDocument().Shutdown();

  EXPECT_CALL(Observer(), ContextDestroyed(&DestDocument()));
  Observer().DidMoveToNewExecutionContext(&DestDocument());
}

}  // namespace blink
