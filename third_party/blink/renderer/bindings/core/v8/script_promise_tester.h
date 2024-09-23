// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_TESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_TESTER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ScriptState;
class ScriptPromiseUntyped;

// Utility for writing unit tests involving promises.
// Typical usage:
//   ScriptPromiseTester tester(script_state, script_promise);
//   tester.WaitUntilSettled();  // Runs a nested event loop.
//   EXPECT_TRUE(tester.IsFulfilled());
//   EXPECT_TRUE(tester.Value().IsUndefined());
class ScriptPromiseTester final {
  STACK_ALLOCATED();

 public:
  ScriptPromiseTester(ScriptState*, ScriptPromiseUntyped);

  ScriptPromiseTester(const ScriptPromiseTester&) = delete;
  ScriptPromiseTester& operator=(const ScriptPromiseTester&) = delete;

  // Run microtasks and tasks until the promise is either fulfilled or rejected.
  // If the promise never settles this will busy loop until the test times out.
  void WaitUntilSettled();

  // Did the promise fulfill?
  bool IsFulfilled() const { return state_ == State::kFulfilled; }

  // Did the promise reject?
  bool IsRejected() const { return state_ == State::kRejected; }

  // The value the promise fulfilled or rejected with.
  ScriptValue Value() const;

 private:
  class ThenFunction;

  using ScriptValueObject = DisallowNewWrapper<ScriptValue>;

  enum class State { kNotSettled, kFulfilled, kRejected };

  ScriptState* script_state_;
  State state_ = State::kNotSettled;
  // Keep ScriptValue explicitly alive via stack-bound root. This allows running
  // tests with `ScriptPromiseTester` that pump the message loop and invoke GCs
  // without stack.
  Persistent<ScriptValueObject> value_object_;

  base::WeakPtrFactory<ScriptPromiseTester> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_TESTER_H_
