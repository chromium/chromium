// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_CONTEXT_MOJO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_CONTEXT_MOJO_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ML;
class MLContextOptions;
class ScriptPromiseResolver;

// The `Mojo` in the class name means this context is backed by a service
// running outside of Blink.
class MODULES_EXPORT MLContextMojo : public MLContext {
 public:
  // Create and build a MLContextMojo object. Resolve the promise with
  // this concrete object if the context is created successfully out of renderer
  // process. Launch WebNN service and bind `WebNNContext` mojo interface
  // to create `WebNNContext` message pipe if needed.
  static void ValidateAndCreateAsync(ScriptPromiseResolver* resolver,
                                     MLContextOptions* options,
                                     ML* ml);

  static MLContext* ValidateAndCreateSync(ScriptState* script_state,
                                          ExceptionState& exception_state,
                                          MLContextOptions* options,
                                          ML* ml);

  MLContextMojo(const V8MLDevicePreference device_preference,
                const V8MLDeviceType device_type,
                const V8MLPowerPreference power_preference,
                const V8MLModelFormat model_format,
                const unsigned int num_threads,
                ML* ml);

  ~MLContextMojo() override;

  void Trace(Visitor* visitor) const override;

  // Creates platform specific graph synchronously in the caller's thread. Once
  // the platform graph is compiled, it should return a concrete MLGraph object.
  // Otherwise, it should return a nullptr and throw a DOMException accordingly.
  void CreateWebNNGraph(
      webnn::mojom::blink::GraphInfoPtr graph_info,
      webnn::mojom::blink::WebNNContext::CreateGraphCallback callback);

 protected:
  // Create `WebNNContext` message pipe with `ML` mojo interface, then
  // create the context with the hardware accelerated OS machine
  // learning API in the WebNN Service.
  void CreateAsyncImpl(ScriptPromiseResolver* resolver,
                       MLContextOptions* options) override;

  // Create `WebNNContext` message pipe with `ML` mojo interface, then
  // create the context with the hardware accelerated OS machine
  // learning API in the WebNN Service.
  MLContext* CreateSyncImpl(ScriptState* script_state,
                            MLContextOptions* options,
                            ExceptionState& exception_state) override;

 private:
  // The callback of creating `WebNNContext` mojo interface from WebNN Service.
  // Return `CreateContextResult::kNotSupported` on non-supported input
  // configuration.
  void OnCreateWebNNContext(ScriptPromiseResolver* resolver,
                            webnn::mojom::blink::CreateContextResultPtr result);

  // The `WebNNContext` is a initialized context that can be used by the
  // hardware accelerated OS machine learning API.
  HeapMojoRemote<webnn::mojom::blink::WebNNContext> remote_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_CONTEXT_MOJO_H_
