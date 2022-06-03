// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/lock_screen/lock_screen_data.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

const char LockScreenData::kSupplementName[] = "LockScreenData";

LockScreenData::LockScreenData(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

LockScreenData::~LockScreenData() = default;

ScriptPromise LockScreenData::getLockScreenData(ScriptState* script_state,
                                                LocalDOMWindow& window) {
  LockScreenData* supplement =
      Supplement<LocalDOMWindow>::From<LockScreenData>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<LockScreenData>(window);
    ProvideTo(window, supplement);
  }
  return supplement->GetLockScreenData(script_state);
}

ScriptPromise LockScreenData::GetLockScreenData(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  resolver->Resolve(this);
  return promise;
}

ScriptPromise LockScreenData::getKeys(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO(crbug.com/1006642): This should call out to a mojo service.
  resolver->Reject("Not implemented");
  return resolver->Promise();
}

ScriptPromise LockScreenData::getData(ScriptState* script_state,
                                      const String& key) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO(crbug.com/1006642): This should call out to a mojo service.
  resolver->Reject("Not implemented");
  return resolver->Promise();
}

ScriptPromise LockScreenData::setData(ScriptState* script_state,
                                      const String& key,
                                      const String& data) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO(crbug.com/1006642): This should call out to a mojo service.
  resolver->Reject("Not implemented");
  return resolver->Promise();
}

ScriptPromise LockScreenData::deleteData(ScriptState* script_state,
                                         const String& key) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO(crbug.com/1006642): This should call out to a mojo service.
  resolver->Reject("Not implemented");
  return resolver->Promise();
}

void LockScreenData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
