// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LOCK_SCREEN_LOCK_SCREEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LOCK_SCREEN_LOCK_SCREEN_DATA_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalDOMWindow;
class ScriptState;

class LockScreenData final : public ScriptWrappable,
                             public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  explicit LockScreenData(LocalDOMWindow&);
  ~LockScreenData() override;
  static ScriptPromise getLockScreenData(ScriptState*, LocalDOMWindow&);
  ScriptPromise GetLockScreenData(ScriptState*);

  // IDL Interface:
  ScriptPromise getKeys(ScriptState*);
  ScriptPromise getData(ScriptState*, const String& key);
  ScriptPromise setData(ScriptState*, const String& key, const String& data);
  ScriptPromise deleteData(ScriptState*, const String& key);

  void Trace(Visitor* visitor) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LOCK_SCREEN_LOCK_SCREEN_DATA_H_
