// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_H_

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class ScriptState;

class MODULES_EXPORT MLModel final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLModel(
      ExecutionContext* context,
      mojo::PendingRemote<ml::model_loader::mojom::blink::Model> pending_remote,
      ml::model_loader::mojom::blink::ModelInfoPtr model_info);

  ~MLModel() override;

  // IDL Interface:
  ScriptPromise compute(
      ScriptState* script_state,
      const HeapVector<std::pair<String, Member<MLTensor>>>& inputs,
      ExceptionState& exception_state);
  HeapVector<Member<MLTensorInfo>> inputs(ScriptState* script_state);
  HeapVector<Member<MLTensorInfo>> outputs(ScriptState* script_state);

  void Trace(Visitor* visitor) const override;

 private:
  void OnComputeResult(
      ScriptState* script_state,
      ScriptPromiseResolver* resolver,
      ml::model_loader::mojom::blink::ComputeResult result,
      const absl::optional<HashMap<String, Vector<uint8_t>>>& outputs);

  HeapMojoRemote<ml::model_loader::mojom::blink::Model> remote_model_;

  HashMap<String, ml::model_loader::mojom::blink::TensorInfoPtr>
      input_tensor_name_to_info_;
  HashMap<String, ml::model_loader::mojom::blink::TensorInfoPtr>
      output_tensor_name_to_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_H_
