// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_H_

#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_access_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_protocol.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_reader_state.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
class SmartCardResourceManager;

class MODULES_EXPORT SmartCardReader
    : public EventTargetWithInlineData,
      public ExecutionContextLifecycleObserver,
      public ActiveScriptWrappable<SmartCardReader> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using SmartCardReaderInfoPtr = mojom::blink::SmartCardReaderInfoPtr;

  SmartCardReader(SmartCardResourceManager* resource_manager,
                  SmartCardReaderInfoPtr info,
                  ExecutionContext* context);
  ~SmartCardReader() override;

  // SmartCardReader idl
  const String& name() const { return name_; }
  V8SmartCardReaderState state() const { return state_; }
  ScriptPromise connect(ScriptState* script_state,
                        V8SmartCardAccessMode access_mode,
                        ExceptionState& exception_state);
  ScriptPromise connect(ScriptState* script_state,
                        V8SmartCardAccessMode access_mode,
                        const Vector<V8SmartCardProtocol>& preferred_prototols,
                        ExceptionState& exception_state);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange, kStatechange)

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ScriptWrappable:
  bool HasPendingActivity() const override;
  void Trace(Visitor*) const override;

  // ContextLifecycleObserver:
  void ContextDestroyed() override;

  void UpdateInfo(SmartCardReaderInfoPtr info);

 private:
  void OnConnectDone(ScriptPromiseResolver* resolver,
                     device::mojom::blink::SmartCardConnectResultPtr result);

  Member<SmartCardResourceManager> resource_manager_;
  WTF::String name_;
  V8SmartCardReaderState state_;
  WTF::Vector<uint8_t> atr_;

  HeapHashSet<Member<ScriptPromiseResolver>> connect_promises_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_H_
