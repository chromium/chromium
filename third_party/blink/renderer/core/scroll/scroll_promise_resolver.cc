// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_promise_resolver.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ScrollPromiseResolver::ScrollPromiseResolver(ScriptState* script_state) {
  if (script_state &&
      RuntimeEnabledFeatures::ProgrammaticScrollPromiseEnabled()) {
    resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<ScrollResult>>(script_state);
  }
}

ScrollPromiseResolver::~ScrollPromiseResolver() {
  CHECK(script_promise_is_created_);
  CHECK_EQ(num_active_scrolls_, 0U);
}

void ScrollPromiseResolver::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
}

std::unique_ptr<ScrollPromiseResolver::ActiveScrollTracker>
ScrollPromiseResolver::CreateActiveScrollTracker() {
  CHECK(!script_promise_is_created_);
  num_active_scrolls_++;
  return std::make_unique<ActiveScrollTracker>(this);
}

ScriptPromise<ScrollResult> ScrollPromiseResolver::CreateScriptPromise() {
  CHECK(!script_promise_is_created_);
  script_promise_is_created_ = true;
  if (!resolver_) {
    return EmptyPromise();
  }
  ResolvePromiseIfIdle();
  return resolver_->Promise();
}

void ScrollPromiseResolver::ActiveScrollTrackerRemoved() {
  CHECK_GT(num_active_scrolls_, 0U);
  --num_active_scrolls_;
  if (script_promise_is_created_ && resolver_) {
    ResolvePromiseIfIdle();
  }
}

void ScrollPromiseResolver::ResolvePromiseIfIdle() {
  CHECK(script_promise_is_created_);
  CHECK(resolver_);

  if (num_active_scrolls_ > 0) {
    // Not idle yet, so defer resolving the promise.
    return;
  }

  auto* execution_context = resolver_->GetExecutionContext();
  if (!execution_context) {
    // When the execution context is gone, not resolving the promise is fine
    // because JS can't be waiting on it. In fact we can't even resolve it,
    // see https://crbug.com/504073879.
    return;
  }

  execution_context->GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE,
                 blink::BindOnce(
                     [](ScriptPromiseResolver<ScrollResult>* resolver,
                        bool scroll_is_interrupted) {
                       auto* result = ScrollResult::Create();
                       result->setInterrupted(scroll_is_interrupted);
                       resolver->Resolve(result);
                     },
                     Persistent<ScriptPromiseResolver<ScrollResult>>(resolver_),
                     scroll_is_interrupted_));
}

}  // namespace blink
