// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LAYOUT_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/keyboard/keyboard_layout_map.h"

namespace blink {

class ScriptPromiseResolver;

class KeyboardLayout final : public GarbageCollected<KeyboardLayout>,
                             public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(KeyboardLayout);

 public:
  explicit KeyboardLayout(ExecutionContext*);
  virtual ~KeyboardLayout() = default;

  ScriptPromise GetKeyboardLayoutMap(ScriptState*);

  // ContextLifecycleObserver override.
  void Trace(blink::Visitor*) override;

 private:
  // Returns true if the local frame is attached to the renderer.
  bool IsLocalFrameAttached();

  // Returns true if |service_| is initialized and ready to be called.
  bool EnsureServiceConnected();

  // Returns true if the current frame is a top-level browsing context.
  bool CalledFromSupportedContext(ExecutionContext*);

  void GotKeyboardLayoutMap(ScriptPromiseResolver*,
                            mojom::blink::GetKeyboardLayoutMapResultPtr);

  Member<ScriptPromiseResolver> script_promise_resolver_;

  mojo::Remote<mojom::blink::KeyboardLockService> service_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayout);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LAYOUT_H_
