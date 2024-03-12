// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_DML_CONTEXT_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/config/gpu_feature_info.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn::dml {

class Adapter;
class CommandRecorder;

// `ContextImpl` is created by `WebNNContextProviderImpl` and responsible for
// creating `GraphImpl` and `BufferImpl` of DirectML backend for Windows
// platform. The `Adapter` instance is shared by all `GraphImpl` and
// `BufferImpl` created by this context.
class ContextImpl final : public WebNNContextImpl {
 public:
  ContextImpl(scoped_refptr<Adapter> adapter,
              mojo::PendingReceiver<mojom::WebNNContext> receiver,
              WebNNContextProviderImpl* context_provider,
              std::unique_ptr<CommandRecorder> command_recorder,
              const gpu::GpuFeatureInfo& gpu_feature_info);

  ContextImpl(const WebNNContextImpl&) = delete;
  ContextImpl& operator=(const ContextImpl&) = delete;

  ~ContextImpl() override;

 private:
  void CreateGraphImpl(mojom::GraphInfoPtr graph_info,
                       CreateGraphCallback callback) override;

  std::unique_ptr<WebNNBufferImpl> CreateBufferImpl(
      mojo::PendingReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override;

  // The `Adapter` instance shared by all `GraphImpl` created by this context.
  scoped_refptr<Adapter> adapter_;

  // The `CommandRecorder` instance used exclusively by this context.
  std::unique_ptr<CommandRecorder> command_recorder_;

  const raw_ref<const gpu::GpuFeatureInfo> gpu_feature_info_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
