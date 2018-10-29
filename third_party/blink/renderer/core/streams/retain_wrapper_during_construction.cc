// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// This function is defined here, rather than in platform/bindings, because it
// needs to use ScriptValue and ExecutionContext.
bool RetainWrapperDuringConstruction(ScriptWrappable* script_wrapper,
                                     ScriptState* script_state) {
  const auto noop = [](ScriptValue) -> void {};
  auto task_runner = ExecutionContext::From(script_state)
                         ->GetTaskRunner(TaskType::kInternalDefault);
  ScriptValue v8_wrapper(script_state, ToV8(script_wrapper, script_state));
  bool post_task_succeeded =
      task_runner->PostTask(FROM_HERE, WTF::Bind(noop, std::move(v8_wrapper)));
  return post_task_succeeded;
}

}  // namespace blink
