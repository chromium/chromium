// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_H_

#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class CrosWindow;
class CrosScreen;
class ScriptPromiseResolver;

class CrosWindowManagement : public EventTarget,
                             public mojom::blink::CrosWindowManagementObserver,
                             public Supplement<ExecutionContext>,
                             public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static CrosWindowManagement& From(ExecutionContext&);

  explicit CrosWindowManagement(ExecutionContext&);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // GC
  void Trace(Visitor*) const override;

  // Returns the remote for communication with the browser's window management
  // implementation. Returns null if the ExecutionContext is being destroyed.
  mojom::blink::CrosWindowManagement* GetCrosWindowManagementOrNull();

  ScriptPromise getWindows(ScriptState* script_state);
  void WindowsCallback(ScriptPromiseResolver* resolver,
                       WTF::Vector<mojom::blink::CrosWindowInfoPtr> windows);

  ScriptPromise getScreens(ScriptState* script_state);
  void ScreensCallback(ScriptPromiseResolver* resolver,
                       WTF::Vector<mojom::blink::CrosScreenInfoPtr> screens);

  const HeapVector<Member<CrosWindow>>& windows();

  // mojom::blink::CrosWindowManagementObserver
  void DispatchStartEvent() override;
  void DispatchAcceleratorEvent(
      mojom::blink::AcceleratorEventPtr event) override;
  void DispatchWindowOpenedEvent(
      mojom::blink::CrosWindowInfoPtr window) override;
  void DispatchWindowClosedEvent(
      mojom::blink::CrosWindowInfoPtr window) override;

 private:
  HeapMojoRemote<mojom::blink::CrosWindowManagementFactory>
      cros_window_management_factory_{GetExecutionContext()};
  HeapMojoAssociatedRemote<mojom::blink::CrosWindowManagement>
      cros_window_management_{GetExecutionContext()};
  HeapMojoAssociatedReceiver<mojom::blink::CrosWindowManagementObserver,
                             CrosWindowManagement>
      observer_receiver_{this, GetExecutionContext()};

  HeapVector<Member<CrosWindow>> windows_;
  HeapVector<Member<CrosScreen>> screens_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_H_
