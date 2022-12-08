// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_PRESENCE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_PRESENCE_OBSERVER_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT SmartCardReaderPresenceObserver
    : public EventTargetWithInlineData,
      public ExecutionContextLifecycleObserver,
      public ActiveScriptWrappable<SmartCardReaderPresenceObserver> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SmartCardReaderPresenceObserver(ExecutionContext* context);
  ~SmartCardReaderPresenceObserver() override;

  // SmartCardReaderPresenceObserver idl
  DEFINE_ATTRIBUTE_EVENT_LISTENER(readeradd, kReaderadd)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(readerremove, kReaderremove)

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ScriptWrappable:
  bool HasPendingActivity() const override;
  void Trace(Visitor*) const override;

  // ContextLifecycleObserver:
  void ContextDestroyed() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_PRESENCE_OBSERVER_H_
