// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_WINDOW_SCREENS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_WINDOW_SCREENS_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class Screens;

// Supplements LocalDOMWindow with a Screens interface.
// https://github.com/webscreens/window-placement
class WindowScreens final : public GarbageCollected<WindowScreens>,
                            public ExecutionContextLifecycleObserver,
                            public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit WindowScreens(LocalDOMWindow* window);

  // Web-exposed interface:
  static ScriptPromise getScreens(ScriptState* script_state,
                                  LocalDOMWindow& window,
                                  ExceptionState& exception_state);

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  void Trace(Visitor* visitor) const override;

  Screens* screens() { return screens_; }

 private:
  // Returns the supplement, creating one as needed.
  static WindowScreens* From(LocalDOMWindow* window);

  // Requests permission to resolve the returned Screens interface promise.
  ScriptPromise GetScreens(ScriptState* script_state,
                           ExceptionState& exception_state);

  // Handles the permission request result, to reject or resolve the promise.
  void OnPermissionRequestComplete(ScriptPromiseResolver* resolver,
                                   mojom::blink::PermissionStatus status);

  Member<Screens> screens_;
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_WINDOW_SCREENS_H_
