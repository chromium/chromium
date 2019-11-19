// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_

#include <utility>

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

namespace blink {

class ScriptState;

class Clipboard : public EventTargetWithInlineData,
                  public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Clipboard);
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Clipboard(ExecutionContext*);

  ScriptPromise read(ScriptState*);
  ScriptPromise readText(ScriptState*);

  ScriptPromise write(ScriptState*, const HeapVector<Member<ClipboardItem>>&);
  ScriptPromise writeText(ScriptState*, const String&);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(blink::Visitor*) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_
