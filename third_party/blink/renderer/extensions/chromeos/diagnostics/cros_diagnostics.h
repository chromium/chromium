// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_DIAGNOSTICS_CROS_DIAGNOSTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_DIAGNOSTICS_CROS_DIAGNOSTICS_H_

#include "third_party/blink/public/mojom/chromeos/diagnostics/cros_diagnostics.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class CrosCpuInfo;
class CrosNetworkInterface;

class CrosDiagnostics : public ScriptWrappable,
                        public Supplement<ExecutionContext>,
                        public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static CrosDiagnostics& From(ExecutionContext&);

  explicit CrosDiagnostics(ExecutionContext&);

  ScriptPromise<CrosCpuInfo> getCpuInfo(ScriptState* script_state);

  ScriptPromise<IDLSequence<CrosNetworkInterface>> getNetworkInterfaces(
      ScriptState* script_state);

  void Trace(Visitor*) const override;

 private:
  // Diagnostics API implementation. May return null in error cases, e.g. when
  // the ExecutionContext has been deleted.
  mojom::blink::CrosDiagnostics* GetCrosDiagnosticsOrNull();

  void OnGetCpuInfoResponse(ScriptPromiseResolver<CrosCpuInfo>* resolver,
                            mojom::blink::GetCpuInfoResultPtr result);

  void OnGetNetworkInterfacesResponse(
      ScriptPromiseResolver<IDLSequence<CrosNetworkInterface>>* resolver,
      mojom::blink::GetNetworkInterfacesResultPtr result);

  HeapMojoRemote<mojom::blink::CrosDiagnostics> cros_diagnostics_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_DIAGNOSTICS_CROS_DIAGNOSTICS_H_
