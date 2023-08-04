// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_GRAPH_IMPL_H_
#define SERVICES_WEBNN_DML_GRAPH_IMPL_H_

#include <DirectML.h>
#include <wrl.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::dml {

class CommandQueue;
class CommandRecorder;

// GraphImpl inherits WebNNGraphImpl to represent a DML graph implementation. It
// is mainly responsible for building and compiling a DML graph from
// mojom::GraphInfo via GraphBuilder, then initializing and executing the graph
// represented by an IDMLCompiledOperator.
class GraphImpl final : public WebNNGraphImpl {
 public:
  // This method builds and compiles a DML graph from mojom::GraphInfo via
  // GraphBuilder, and then call CommandRecorder::InitializeOperator method to
  // initialize the DML graph. Next, it calls CommandQueue::WaitAsync method to
  // wait for the initialization work to be completed on GPU, the GraphImpl
  // instance will only be created and bound to the mojom receiver in
  // GraphImpl::OnWaitForBuildSignal method.
  static void CreateAndBuild(scoped_refptr<CommandQueue> command_queue,
                             Microsoft::WRL::ComPtr<IDMLDevice> dml_device,
                             const mojom::GraphInfoPtr& graph_info,
                             mojom::WebNNContext::CreateGraphCallback callback);

  GraphImpl(const GraphImpl&) = delete;
  GraphImpl& operator=(const GraphImpl&) = delete;
  ~GraphImpl() override;

 private:
  GraphImpl(std::unique_ptr<CommandRecorder> command_recorder,
            Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer,
            Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator);

  // Create the GraphImpl instance and bind it to the mojom::WebNNGraph
  // receiver, then run callback to send the pending remote to the
  // render.
  // Notice that the persistent_buffer could be nullptr which means it isn't
  // required by the graph.
  static void OnWaitForBuildSignal(
      std::unique_ptr<CommandRecorder> command_recorder,
      Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer,
      Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator,
      mojom::WebNNContext::CreateGraphCallback callback);

  // The persistent buffer will be initialized after the initialization work on
  // GPU is completed and will be used for the following graph executions. It
  // could be nullptr which means it isn't required by the graph and won't need
  // to be bound for graph executions.
  Microsoft::WRL::ComPtr<ID3D12Resource> persistent_buffer_;
  std::unique_ptr<CommandRecorder> command_recorder_;
  // IDMLCompiledOperator represents a compiled and initialized DML graph to be
  // executed on GPU.
  Microsoft::WRL::ComPtr<IDMLCompiledOperator> compiled_operator_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_GRAPH_IMPL_H_
