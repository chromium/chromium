// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_H_

#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
class MODULES_EXPORT SmartCardReader
    : public EventTargetWithInlineData,
      public ExecutionContextLifecycleObserver,
      public ActiveScriptWrappable<SmartCardReader> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using SmartCardReaderInfoPtr = mojom::blink::SmartCardReaderInfoPtr;

  SmartCardReader(SmartCardReaderInfoPtr info, ExecutionContext* context);
  ~SmartCardReader() override;

  // SmartCardReader idl
  const String& name() const;

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
  SmartCardReaderInfoPtr reader_info_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_H_
