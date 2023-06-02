// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_CROS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_CROS_H_

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace blink {

class ScriptPromiseResolver;

class MODULES_EXPORT MLGraphCrOS final : public MLGraph {
 public:
  // Create and build an MLGraphCrOS object. Resolve the promise with
  // this concrete object if the underlying TF-Lite model converted from WebNN
  // graph builds successfully.
  static void ValidateAndBuildAsync(MLContext* context,
                                    const MLNamedOperands& named_outputs,
                                    ScriptPromiseResolver* resolver);

  // The constructor shouldn't be called directly, use ValidateAndBuildAsync()
  // method instead, and the declaration must be public to be called by
  // MakeGarbageCollected.
  MLGraphCrOS(ExecutionContext* execution_context, MLContext* context);
  ~MLGraphCrOS() override;

  void Trace(Visitor* visitor) const override;

  // The caller of this function is responsible to keep flatbuffer alive and
  // unset it when it's no longer used.
  static void SetFlatbufferForTesting(flatbuffers::DetachedBuffer* flatbuffer);

 private:
  // The callback of loading tflite model, it will bind the `Model` pending
  // remote if it's successful.
  void OnRemoteModelLoad(
      ExecutionContext* execution_context,
      ScriptPromiseResolver* resolver,
      ml::model_loader::mojom::blink::LoadModelResult result,
      mojo::PendingRemote<ml::model_loader::mojom::blink::Model> pending_remote,
      ml::model_loader::mojom::blink::ModelInfoPtr model_info);

  // Load a WebNN graph in `MLService` with `ModelLoader` message pipe, the
  // operations of WebNN need to be converted into a TF-Lite model in
  // FlatBuffers.
  void BuildAsyncImpl(const MLNamedOperands& named_outputs,
                      ScriptPromiseResolver* resolver) override;

  // Load the converted model with synchronous call of `ModelLoader` interface.
  MLGraph* BuildSyncImpl(const MLNamedOperands& named_outputs,
                         ExceptionState& exception_state) override;

  // Compute the converted model with asynchronous call of `Model` interface.
  void ComputeAsyncImpl(const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ScriptPromiseResolver* resolver,
                        ExceptionState& exception_state) override;
  // Resolve the promise with an MLComputeResult that contains input and output
  // ArrayBufferViews. The `inputs_info` and `outputs_info` carry the backing
  // memory in `ArrayBufferContents` transferred from the original user supplied
  // `ArrayBufferView`s.
  void OnComputeGraph(
      ScriptPromiseResolver* resolver,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          inputs_info,
      std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
          outputs_info,
      ml::model_loader::mojom::blink::ComputeResult mojo_result,
      const absl::optional<HashMap<String, Vector<uint8_t>>>& mojo_outputs);

  // Compute the converted model with synchronous call of `Model` interface.
  void ComputeSyncImpl(const MLNamedArrayBufferViews& inputs,
                       const MLNamedArrayBufferViews& outputs,
                       ExceptionState& exception_state) override;

  HeapMojoRemote<ml::model_loader::mojom::blink::Model> remote_model_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_CROS_H_
