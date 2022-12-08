// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_RESOURCE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_RESOURCE_MANAGER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class NavigatorBase;

class MODULES_EXPORT SmartCardResourceManager final
    : public ScriptWrappable,
      public Supplement<NavigatorBase>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Getter for navigator.smartCard
  static SmartCardResourceManager* smartCard(NavigatorBase&);

  explicit SmartCardResourceManager(NavigatorBase&);

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  // ScriptWrappable overrides
  void Trace(Visitor*) const override;

  // SmartCardResourceManager idl
  ScriptPromise getReaders(ScriptState* script_state);
  ScriptPromise watchForReaders(ScriptState* script_state,
                                ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_RESOURCE_MANAGER_H_
