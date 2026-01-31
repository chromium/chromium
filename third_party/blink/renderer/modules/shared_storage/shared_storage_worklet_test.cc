// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

// Tests for the document-side SharedStorageWorklet class.
// These tests focus on lifecycle management and promise resolver cleanup.
class SharedStorageWorkletDocumentTest : public PageTestBase {
 public:
  SharedStorageWorkletDocumentTest() = default;
};

TEST_F(SharedStorageWorkletDocumentTest,
       ContextDestroyedDetachesPendingResolvers) {
  // This test verifies that when ContextDestroyed() is called, all pending
  // promise resolvers are properly detached. This addresses crbug.com/40946969.
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);

  auto* worklet =
      MakeGarbageCollected<SharedStorageWorklet>(GetFrame().DomWindow());

  // Create a resolver and manually add it to pending_resolvers_ to simulate
  // a pending operation.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  worklet->pending_resolvers_.insert(resolver);

  EXPECT_EQ(1u, worklet->pending_resolvers_.size());

  // Simulate context destruction.
  GetFrame().DomWindow()->NotifyContextDestroyed();

  // After context destruction, pending resolvers should be cleared.
  EXPECT_EQ(0u, worklet->pending_resolvers_.size());
}

TEST_F(SharedStorageWorkletDocumentTest, FinishOperationRemovesResolver) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);

  auto* worklet =
      MakeGarbageCollected<SharedStorageWorklet>(GetFrame().DomWindow());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  worklet->pending_resolvers_.insert(resolver);

  EXPECT_EQ(1u, worklet->pending_resolvers_.size());

  worklet->FinishOperation(resolver);

  EXPECT_EQ(0u, worklet->pending_resolvers_.size());
}

TEST_F(SharedStorageWorkletDocumentTest, MultipleResolversCleanedUp) {
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);

  auto* worklet =
      MakeGarbageCollected<SharedStorageWorklet>(GetFrame().DomWindow());

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* resolver3 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);

  worklet->pending_resolvers_.insert(resolver1);
  worklet->pending_resolvers_.insert(resolver2);
  worklet->pending_resolvers_.insert(resolver3);

  EXPECT_EQ(3u, worklet->pending_resolvers_.size());

  GetFrame().DomWindow()->NotifyContextDestroyed();

  EXPECT_EQ(0u, worklet->pending_resolvers_.size());
}

}  // namespace blink
