// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_WORKLET_H_

#include "third_party/blink/renderer/core/workers/experimental/task.h"
#include "third_party/blink/renderer/core/workers/worklet.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Document;
class LocalDOMWindow;

class TaskWorklet final : public Worklet,
                          public Supplement<LocalDOMWindow>,
                          public ThreadPoolThreadProvider {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(TaskWorklet);

 public:
  static const char kSupplementName[];
  static TaskWorklet* From(LocalDOMWindow&);

  Task* postTask(ScriptState*,
                 const ScriptValue& task,
                 const Vector<ScriptValue>& arguments);

  Task* postTask(ScriptState*,
                 const String& function_name,
                 const Vector<ScriptValue>& arguments);

  ThreadPoolThread* GetLeastBusyThread() override;

  void Trace(blink::Visitor*) override;

 private:
  explicit TaskWorklet(Document*);
  ~TaskWorklet() override = default;

  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;
  wtf_size_t SelectGlobalScope() final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_WORKLET_H_
