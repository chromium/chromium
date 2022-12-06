// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_H_

#include "components/ml/mojom/ml_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class MLContextOptions;
class ScriptPromise;
class ScriptState;

// This class represents the "Machine Learning" object "navigator.ml" and will
// be shared between the Model Loader API and WebNN API.
class MODULES_EXPORT ML final : public ScriptWrappable,
                                public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ML(ExecutionContext* execution_context);

  ML(const ML&) = delete;
  ML& operator=(const ML&) = delete;

  void CreateModelLoader(
      ScriptState* script_state,
      ExceptionState& exception_state,
      ml::model_loader::mojom::blink::CreateModelLoaderOptionsPtr options,
      ml::model_loader::mojom::blink::MLService::CreateModelLoaderCallback
          callback);

  void Trace(blink::Visitor*) const override;

  // IDL interface:
  ScriptPromise createContext(ScriptState* state,
                              MLContextOptions* option,
                              ExceptionState& exception_state);

 private:
  // Binds the Mojo connection to browser process if needed.
  // Returns false when the execution context is not valid (e.g., the frame is
  // detached) and an exception will be thrown.
  // Otherwise returns true.
  bool BootstrapMojoConnectionIfNeeded(ScriptState* script_state,
                                       ExceptionState& exception_state);

  HeapMojoRemote<ml::model_loader::mojom::blink::MLService> remote_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_H_
