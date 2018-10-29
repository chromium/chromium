// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_CONTROLLER_H_

#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;

class ScriptedTaskQueue;

// This class corresponds to the ScriptedTaskQueueController interface.
class CORE_EXPORT ScriptedTaskQueueController final
    : public ScriptWrappable,
      public ContextLifecycleObserver,
      public Supplement<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ScriptedTaskQueueController);

 public:
  static const char kSupplementName[];

  static ScriptedTaskQueueController* From(Document&);

  ScriptedTaskQueue* defaultQueue(const String&);

  void Trace(blink::Visitor*) override;

 private:
  explicit ScriptedTaskQueueController(ExecutionContext*);

  HeapHashMap<String, TraceWrapperMember<ScriptedTaskQueue>> task_queues_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCRIPTED_TASK_QUEUE_CONTROLLER_H_
