// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ML;
class MLModelLoader;

class MODULES_EXPORT MLContext final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLContext(const V8MLDevicePreference device_preference,
            const V8MLPowerPreference power_preference,
            const V8MLModelFormat model_format,
            const unsigned int num_threads,
            ML* ml);

  MLContext(const MLContext&) = delete;
  MLContext& operator=(const MLContext&) = delete;

  ~MLContext() override;

  V8MLDevicePreference GetDevicePreference() const;
  V8MLPowerPreference GetPowerPreference() const;
  V8MLModelFormat GetModelFormat() const;
  unsigned int GetNumThreads() const;
  void LogConsoleWarning(const String& message);

  ML* GetML();
  // This method returns a MLModelLoader that's used and shared by WebNN APIs
  // invoked on this MLContext.
  MLModelLoader* GetModelLoaderForWebNN(ScriptState* script_state);

  void Trace(Visitor* visitor) const override;

  // IDL interface:
  ScriptPromise compute(ScriptState* script_state,
                        MLGraph* graph,
                        const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ExceptionState& exception_state);

  void computeSync(MLGraph* graph,
                   const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ExceptionState& exception_state);

  void CreateWebNNGraph(
      ScriptState* script_state,
      webnn::mojom::blink::GraphInfoPtr graph_info,
      webnn::mojom::blink::WebNNContext::CreateGraphCallback callback);

 private:
  V8MLDevicePreference device_preference_;
  V8MLPowerPreference power_preference_;
  V8MLModelFormat model_format_;
  unsigned int num_threads_;

  Member<ML> ml_;
  // WebNN uses this MLModelLoader to build a computational graph.
  Member<MLModelLoader> ml_model_loader_;

  // The callback of creating context called from WebNN server side.
  void OnCreateWebNNContext(
      ScriptState* script_state,
      webnn::mojom::blink::GraphInfoPtr graph_info,
      webnn::mojom::blink::WebNNContext::CreateGraphCallback callback,
      webnn::mojom::blink::CreateContextResultPtr result);
  // WebNN support multiple types of neural network inference hardware
  // acceleration, the context of WebNN in server side is used to map different
  // device and represent a state of graph execution processes.
  HeapMojoRemote<webnn::mojom::blink::WebNNContext> webnn_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
