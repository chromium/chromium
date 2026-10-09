// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_microtasks_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8-data.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-local-handle.h"

namespace blink {

TEST(V8MicrotasksScopeTest, RunMicrotasks) {
  bool microtask_run = false;

  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::MicrotaskQueue* queue = scope.GetContext()->GetMicrotaskQueue();
  queue->EnqueueMicrotask(
      scope.GetIsolate(),
      [](v8::Local<v8::Data> data) {
        *static_cast<bool*>(data.As<v8::External>()->Value(
            v8::kExternalPointerTypeTagDefault)) = true;
      },
      v8::External::New(scope.GetIsolate(), &microtask_run,
                        v8::kExternalPointerTypeTagDefault));

  EXPECT_FALSE(microtask_run);
  {
    V8RunMicrotasksScope microtasks_scope(scope.GetScriptState());
    EXPECT_FALSE(microtask_run);
  }
  EXPECT_TRUE(microtask_run);
}

TEST(V8MicrotasksScopeTest, DoNotRunMicrotasks) {
  bool microtask_run = false;

  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::MicrotaskQueue* queue = scope.GetContext()->GetMicrotaskQueue();
  queue->EnqueueMicrotask(
      scope.GetIsolate(),
      [](v8::Local<v8::Data> data) {
        *static_cast<bool*>(data.As<v8::External>()->Value(
            v8::kExternalPointerTypeTagDefault)) = true;
      },
      v8::External::New(scope.GetIsolate(), &microtask_run,
                        v8::kExternalPointerTypeTagDefault));

  EXPECT_FALSE(microtask_run);
  {
    V8DoNotRunMicrotasksScope microtasks_scope(scope.GetScriptState());
  }
  EXPECT_FALSE(microtask_run);
}

}  // namespace blink
