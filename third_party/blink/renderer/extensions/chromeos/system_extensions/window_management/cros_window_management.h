// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_H_

#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class ScriptPromiseResolver;

class CrosWindowManagement : public ScriptWrappable,
                             public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CrosWindowManagement(ExecutionContext* execution_context);

  void Trace(Visitor*) const override;

  // Returns the remote for communication with the browser's window management
  // implementation. May return null in error cases.
  mojom::blink::CrosWindowManagement* GetCrosWindowManagementOrNull();

  ScriptPromise windows(ScriptState* script_state);
  void WindowsCallback(ScriptPromiseResolver* resolver,
                       WTF::Vector<mojom::blink::CrosWindowPtr> windows);

 private:
  HeapMojoRemote<mojom::blink::CrosWindowManagement> cros_window_management_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_H_
