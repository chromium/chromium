// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_resource_manager.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const char SmartCardResourceManager::kSupplementName[] =
    "SmartCardResourceManager";

SmartCardResourceManager* SmartCardResourceManager::smartCard(
    NavigatorBase& navigator) {
  SmartCardResourceManager* smartcard =
      Supplement<NavigatorBase>::From<SmartCardResourceManager>(navigator);
  if (!smartcard) {
    smartcard = MakeGarbageCollected<SmartCardResourceManager>(navigator);
    ProvideTo(navigator, smartcard);
  }
  return smartcard;
}

SmartCardResourceManager::SmartCardResourceManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()) {}

void SmartCardResourceManager::ContextDestroyed() {
  NOTIMPLEMENTED();
}

void SmartCardResourceManager::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

ScriptPromise SmartCardResourceManager::getReaders(ScriptState* script_state) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise SmartCardResourceManager::watchForReaders(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

}  // namespace blink
