// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include "base/task/single_thread_task_runner.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MLGraph::MLGraph(ExecutionContext* execution_context,
                 MLContext* context,
                 mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraph>
                     pending_graph_remote,
                 blink::WebNNGraphToken graph_token,
                 NamedOperandDescriptors input_constraints,
                 NamedOperandDescriptors output_constraints,
                 Vector<V8MLDeviceType> devices,
                 base::PassKey<MLGraphBuilder> /*pass_key*/)
    : input_constraints_(std::move(input_constraints)),
      output_constraints_(std::move(output_constraints)),
      ml_context_(context),
      graph_token_(graph_token),
      remote_graph_(execution_context),
      devices_(std::move(devices)) {
  // Bind the end point of `WebNNGraph` mojo interface in the blink side.
  remote_graph_.Bind(
      std::move(pending_graph_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_graph_.set_disconnect_handler(
      BindOnce(&MLGraph::OnConnectionError, WrapWeakPersistent(this)));
}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_graph_);
  ScriptWrappable::Trace(visitor);
}

void MLGraph::destroy() {
  if (remote_graph_.is_bound()) {
    OnConnectionError();
  }
}

bool MLGraph::IsDestroyed() const {
  return !remote_graph_.is_bound();
}

Vector<V8MLDeviceType> MLGraph::devices() const {
  return devices_;
}

const MLGraph::NamedOperandDescriptors& MLGraph::GetInputConstraints() const {
  return input_constraints_;
}

const MLGraph::NamedOperandDescriptors& MLGraph::GetOutputConstraints() const {
  return output_constraints_;
}

const MLContext* MLGraph::Context() const {
  return ml_context_.Get();
}

void MLGraph::OnConnectionError() {
  remote_graph_.reset();
}

}  // namespace blink
