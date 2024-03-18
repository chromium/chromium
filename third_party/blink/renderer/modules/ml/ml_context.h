// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ML;
class MLBuffer;
class MLBufferDescriptor;
class MLComputeResult;
class MLContextOptions;

class MODULES_EXPORT MLContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Resolves `resolver` with a newly created MLContext. The caller must call
  // `Promise()` on `resolver` before calling this method.
  static void ValidateAndCreate(ScriptPromiseResolverTyped<MLContext>* resolver,
                                MLContextOptions* options,
                                ML* ml);

  // The constructor shouldn't be called directly. The callers should use the
  // ValidateAndCreate() method instead.
  MLContext(const V8MLDevicePreference device_preference,
            const V8MLDeviceType device_type,
            const V8MLPowerPreference power_preference,
            const V8MLModelFormat model_format,
            const unsigned int num_threads,
            ML* ml);

  MLContext(const MLContext&) = delete;
  MLContext& operator=(const MLContext&) = delete;

  ~MLContext() override;

  V8MLDevicePreference GetDevicePreference() const;
  V8MLDeviceType GetDeviceType() const;
  V8MLPowerPreference GetPowerPreference() const;
  V8MLModelFormat GetModelFormat() const;
  unsigned int GetNumThreads() const;
  void LogConsoleWarning(const String& message);

  ML* GetML();

  void Trace(Visitor* visitor) const override;

  // IDL interface:
  ScriptPromiseTyped<MLComputeResult> compute(
      ScriptState* script_state,
      MLGraph* graph,
      const MLNamedArrayBufferViews& inputs,
      const MLNamedArrayBufferViews& outputs,
      ExceptionState& exception_state);

  MLBuffer* createBuffer(ScriptState* script_state,
                         const MLBufferDescriptor* descriptor,
                         ExceptionState& exception_state);

  // Creates a platform-specific compute graph described by `graph_info`.
  void CreateWebNNGraph(
      webnn::mojom::blink::GraphInfoPtr graph_info,
      webnn::mojom::blink::WebNNContext::CreateGraphCallback callback);

  // Creates platform specific buffer described by `buffer_info`.
  void CreateWebNNBuffer(
      mojo::PendingReceiver<webnn::mojom::blink::WebNNBuffer> receiver,
      webnn::mojom::blink::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle);

 private:
  // The callback of creating `WebNNContext` mojo interface from WebNN Service.
  // Return `CreateContextResult::kNotSupported` on non-supported input
  // configuration.
  void OnCreateWebNNContext(ScopedMLTrace scoped_trace,
                            ScriptPromiseResolverTyped<MLContext>* resolver,
                            webnn::mojom::blink::CreateContextResultPtr result);

  V8MLDevicePreference device_preference_;
  V8MLDeviceType device_type_;
  V8MLPowerPreference power_preference_;
  V8MLModelFormat model_format_;
  unsigned int num_threads_;

  Member<ML> ml_;

  // The `WebNNContext` is a initialized context that can be used by the
  // hardware accelerated OS machine learning API.
  HeapMojoRemote<webnn::mojom::blink::WebNNContext> remote_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
