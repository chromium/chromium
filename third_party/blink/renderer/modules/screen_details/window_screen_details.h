// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_DETAILS_WINDOW_SCREEN_DETAILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_DETAILS_WINDOW_SCREEN_DETAILS_H_

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
class ScreenDetails;

// Supplements LocalDOMWindow with a ScreenDetails interface.
// https://w3c.github.io/window-placement/
class WindowScreenDetails final : public GarbageCollected<WindowScreenDetails>,
                                  public ExecutionContextLifecycleObserver,
                                  public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit WindowScreenDetails(LocalDOMWindow* window);

  // Web-exposed interface:
  static ScriptPromise getScreenDetails(ScriptState* script_state,
                                        LocalDOMWindow& window,
                                        ExceptionState& exception_state);

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  void Trace(Visitor* visitor) const override;

  ScreenDetails* screen_details() { return screen_details_; }

 private:
  // Returns the supplement, creating one as needed.
  static WindowScreenDetails* From(LocalDOMWindow* window);

  // Requests permission to resolve the returned Screens interface promise.
  ScriptPromise GetScreenDetails(ScriptState* script_state,
                                 ExceptionState& exception_state);

  // Handles the permission request result, to reject or resolve the promise.
  void OnPermissionRequestComplete(ScriptPromiseResolver* resolver,
                                   mojom::blink::PermissionStatus status);

  Member<ScreenDetails> screen_details_;
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_DETAILS_WINDOW_SCREEN_DETAILS_H_
